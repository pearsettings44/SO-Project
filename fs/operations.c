#include "operations.h"
#include "config.h"
#include "utils.h"
#include "state.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "betterassert.h"

#define BLOCK_SIZE state_block_size()

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }

    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t *root_inode) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // check if given a directory's inode 
    if (root_inode->i_node_type != T_DIRECTORY) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    if (root_dir_inode == NULL) {
        return -1;
    }

    pthread_rwlock_t *root_dir_rwl = inode_rwl_get(ROOT_DIR_INUM);
    // lock root dir to avoid changes mid write (creation of duplicate files)
    rwl_wrlock(root_dir_rwl);
    
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        // file already created, let go of root dir lock
        rwl_unlock(root_dir_rwl);
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");
        
        pthread_rwlock_t *inode_rwl = inode_rwl_get(inum);
        // lock inode
        rwl_wrlock(inode_rwl);

        if (inode->i_node_type == T_LINK) {
            // symlinks don't support O_CREATE flags
            if (mode & TFS_O_CREAT) {
                rwl_unlock(inode_rwl);
                return -1;
            }

            // get pathname of file pointed to by this symlink
            void *block = data_block_get(inode->i_data_block);
            char buffer[MAX_FILE_NAME];
            memcpy(buffer, block, strlen((char *)block) + 1);

            // unlock inode after data being read
            rwl_unlock(inode_rwl);
            int fd = tfs_open(buffer, mode);
            // if dangled link
            if (fd == - 1) {
                return -1;
            }

            return fd;
        }

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
        rwl_unlock(inode_rwl);
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            rwl_unlock(root_dir_rwl);
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            rwl_unlock(root_dir_rwl);
            return -1; // no space in directory
        }
        rwl_unlock(root_dir_rwl);
        offset = 0;
    } else {
        rwl_unlock(root_dir_rwl);
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    // check if link_name is valid
    if (!valid_pathname(link_name)) {
        return -1;
    }

    // root directory inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    pthread_rwlock_t *root_lock = inode_rwl_get(ROOT_DIR_INUM);

    // lock root inode to deny changes to dir mid operation
    rwl_wrlock(root_lock);

    // check if target exists
    int target_inumber = tfs_lookup(target, root_dir_inode);
    if (target_inumber == -1) {
        rwl_unlock(root_lock);
        return -1;
    }

    // check if a file with link_name already exists 
    if (tfs_lookup(link_name, root_dir_inode) != -1) {
        rwl_unlock(root_lock);
        return -1;
    }

    // create a link type inode
    int new_inum = inode_create(T_LINK);
    // no space in inode table
    if (new_inum == -1) {
        rwl_unlock(root_lock);
        return -1;
    }

    // get created inode
    inode_t *new_inode = inode_get(new_inum);
    // allocate new block
    int new_bnum = data_block_alloc();
    // if no free blocks
    if (new_bnum == -1) {
        inode_delete(new_inum);
        rwl_unlock(root_lock);
        return -1;
    }

    // get block pointer
    void *block = data_block_get(new_bnum);
    // copy target path into block
    memcpy(block, target, strlen(target) + 1);

    // associate created block to link's inode
    new_inode->i_data_block = new_bnum;

    // add entry to dir and undo operations if no entries left on dir
    if (add_dir_entry(root_dir_inode, link_name + 1, new_inum) == -1) {
        inode_delete(new_inum);
        rwl_unlock(root_lock);
        return -1;
    }

    rwl_unlock(root_lock);
    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    // check if link_name is valid
    if (!valid_pathname(link_name)) {
        return -1;
    }

    // root directory inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    pthread_rwlock_t *root_lock = inode_rwl_get(ROOT_DIR_INUM);

    // lock root inode to deny changes to dir mid operation
    rwl_wrlock(root_lock);

    int target_inumber = tfs_lookup(target, root_dir_inode);
    // target doesn't exist
    if (target_inumber == -1) {
        rwl_unlock(root_lock);
        return -1;
    }

    // check if a file with link_name already exists
    if (tfs_lookup(link_name, root_dir_inode) != -1) {
        rwl_unlock(root_lock);
        return -1;
    }

    inode_t *target_inode = inode_get(target_inumber);
    pthread_rwlock_t *target_inode_lock = inode_rwl_get(target_inumber);

    // lock target file inode
    rwl_wrlock(target_inode_lock);
    
    // cannot hardlink to symlink (stated in paper)
    if (target_inode->i_node_type == T_LINK) {
        rwl_unlock(target_inode_lock);
        rwl_unlock(root_lock);
        return -1;
    }

    // add link to dir entry and returns error value if no entries left on dir
    if (add_dir_entry(root_dir_inode, link_name + 1, target_inumber) == -1) {
        rwl_unlock(target_inode_lock);
        rwl_unlock(root_lock);
        return -1;
    }

    // increment target inode's link counter
    target_inode->i_links_count++;

    rwl_unlock(target_inode_lock);
    rwl_unlock(root_lock);

    return 0;
}

int tfs_close(int fhandle) {
    // Get the open file table entry
    open_file_entry_t *file = get_open_file_entry(fhandle);
    // If the file is not open, return an error
    if (file == NULL) {
        return -1;
    }
    // If the file is open, remove it from the open file table
    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    // Get the open file table entry
    open_file_entry_t *file = get_open_file_entry(fhandle);
    // If the file is not open, return an error
    if (file == NULL) {
        return -1;
    }

    // lock open file entry
    mutex_lock(&file->lock);

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    // in this implementation we cannot close opened files
    // // ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // lock inode to avoid changes mid write 
    pthread_rwlock_t *inode_lock = inode_rwl_get(file->of_inumber);
    rwl_wrlock(inode_lock);

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                rwl_unlock(inode_lock);
                mutex_unlock(&file->lock);
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        // if data block delete befored acquiring lock
        if (block == NULL) {
            rwl_unlock(inode_lock);
            mutex_unlock(&file->lock);
            return -1;
        }

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }

    }

    rwl_unlock(inode_lock);
    mutex_unlock(&file->lock);

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    // Get the open file table entry
    open_file_entry_t *file = get_open_file_entry(fhandle);
    // If the file is not open, return an error
    if (file == NULL) {
        return -1;
    }

    // lock open file entry
    mutex_lock(&file->lock);

    // cannot delete open file in this implementation
    // // ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");
    
    // From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    // lock inode to avoid changes mid read
    pthread_rwlock_t *inode_lock = inode_rwl_get(file->of_inumber);
    rwl_rdlock(inode_lock);

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        // block was deleted before acquiring the inode lock
        if (block == NULL) {
            rwl_unlock(inode_lock);
            mutex_unlock(&file->lock);
            return -1;
        }

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    rwl_unlock(inode_lock);
    mutex_unlock(&file->lock);
    
    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    // root directory inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    pthread_rwlock_t *root_lock = inode_rwl_get(ROOT_DIR_INUM);

    // lock root directory to avoid changes mid operation
    rwl_wrlock(root_lock);

    int target_inum = tfs_lookup(target, root_dir_inode);
    // target doesn't exist or has invalid name
    if (target_inum == -1) {
        rwl_unlock(root_lock);
        return -1;
    }
    
    // get target's inode
    inode_t *target_inode = inode_get(target_inum);

    // if file is opened, do not allow unlink 
    // symlinks are never in the open file table
    if ((target_inode->i_node_type != T_LINK) && 
        (is_in_open_file_table(target_inum))) {
        rwl_unlock(root_lock);
        return -1;
    }

    // remove target entry in directory
    if (clear_dir_entry(root_dir_inode, target + 1) == -1) {
        rwl_unlock(root_lock);
        return -1;
    }

    // lock file's inode
    pthread_rwlock_t *target_rwl = inode_rwl_get(target_inum);
    rwl_wrlock(target_rwl);
    // if no more links, free inode
    if (target_inode->i_links_count == 1) {
        // if other thread deleted this inode
        if (inode_delete(target_inum) == -1) {
            rwl_unlock(root_lock);
            return -1;
        }
    } else {
        target_inode->i_links_count--;
    }

    rwl_unlock(target_rwl);
    rwl_unlock(root_lock);
    return 0;
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    // open source file
    FILE *file = fopen(source_path, "r");
    // problem opening source file
    if (file == NULL) {
        return -1;
    }
    // buffer with 1025 bytes == block size + 1
    char buffer[BLOCK_SIZE + 1];
    // read entire block size
    size_t bytes_read = fread(buffer, sizeof(*buffer), BLOCK_SIZE + 1, file);
    // problem reading the file or file size exceeds tfs block limit
    if (bytes_read == -1 || bytes_read > BLOCK_SIZE) {
        fclose(file);
        return -1;
    }

    // redirect data to tfs
    int fd = tfs_open(dest_path, TFS_O_TRUNC | TFS_O_CREAT);
    // problem opening dest file in tfs
    if (fd == -1) {
        return -1;
    }
    // write to tfs
    ssize_t bytes_size = tfs_write(fd, buffer, bytes_read);
    // problem writing to dest file in tfs
    if (bytes_size == -1) {
        tfs_close(fd);
        return -1;
    }

    // operations handled, just signals problems on closing files
    if (fclose(file) == EOF || tfs_close(fd) == -1) {
        return -1;
    }


    return 0;
}
