#include "operations.h"
#include "config.h"
#include "state.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int tfs_lookup(char const *name, inode_t const *root_inode) {
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
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        if (inode->i_node_type == T_LINK) {
            // symlinks don't support O_CREATE flags
            if (mode & TFS_O_CREAT) {
                return -1;
            }

            // get pathname of file pointed to by this symlink
            void *block = data_block_get(inode->i_data_block);
            char buffer[MAX_FILE_NAME];
            memcpy(buffer, block, strlen((char *)block) + 1);

            // update inum to pointed file's inum
            inum = tfs_lookup(buffer, root_dir_inode);
            // if dangled link
            if (inum == -1) {
                return -1;
            }
            // update inode to pointed file's inode
            inode = inode_get(inum);
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
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
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
    // root directory inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    // check if target exists
    int target_inumber = tfs_lookup(target, root_dir_inode);
    if (target_inumber == -1) {
        return -1;
    }

    // check if a file with link_name already exists
    if (tfs_lookup(link_name, root_dir_inode) != -1) {
        return -1;
    }

    // create a link type inode
    int new_inum = inode_create(T_LINK);
    // no space in inode table
    if (new_inum == -1) {
        return -1;
    }

    // get created inode
    inode_t *new_inode = inode_get(new_inum);
    // allocate new block
    int new_bnum = data_block_alloc();
    // if no free blocks
    if (new_bnum == -1) {
        inode_delete(new_inum);
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
        data_block_free(new_bnum);
        inode_delete(new_inum);
        return -1;
    }

    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    // root directory inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    // check if target exists
    int target_inumber = tfs_lookup(target, root_dir_inode);
    if (target_inumber == -1) {
        return -1;
    }

    // check if a file with link_name already exists
    if (tfs_lookup(link_name, root_dir_inode) != -1) {
        return -1;
    }

    // cannot hardlink to a symlink (breaks POSIX API)
    if (inode_get(target_inumber)->i_node_type == T_LINK) {
        return -1;
    }

    // add link to dir entry and returns error value if no entries left on dir
    if (add_dir_entry(root_dir_inode, link_name + 1, target_inumber) == -1) {
        return -1;
    }

    // increment inode's link counter
    inode_get(target_inumber)->i_links_count++;

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

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

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
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    // Get the open file table entry
    open_file_entry_t *file = get_open_file_entry(fhandle);
    // If the file is not open, return an error
    if (file == NULL) {
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    // root directory inode
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);

    int target_inum = tfs_lookup(target, root_dir_inode);
    // target doesn't exist or has invalid name
    if (target_inum == -1) {
        return -1;
    }

    // remove target entry
    if (clear_dir_entry(root_dir_inode, target + 1) == -1) {
        return -1;
    }

    inode_t *target_inode = inode_get(target_inum);
    // if no no more links, free inode
    if (target_inode->i_links_count == 1) {
        // if inode has data on associated block, free block
        if (target_inode->i_data_block != -1) {
            data_block_free(target_inode->i_data_block);
        }
        inode_delete(target_inum);
    } else {
        target_inode->i_links_count--;
    }

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
    int dest = tfs_open(dest_path, TFS_O_TRUNC | TFS_O_CREAT);
    // problem opening dest file in tfs
    if (dest == -1) {
        return -1;
    }
    // write to tfs
    ssize_t r = tfs_write(dest, buffer, bytes_read);
    // problem writing to dest file in tfs
    if (r == -1) {
        tfs_close(dest);
        return -1;
    }

    // operations handled, just signals problems on closing files
    if (fclose(file) == EOF || tfs_close(dest) == -1) {
        return -1;
    }

    return 0;
}
