# Tests

Below is a description of what each test does.

## Teacher Provided Tests

- `copy_from_external_empty`: Check if the copy from external FS function works with an empty file.
- `copy_from_external_errors`: Check if the copy from external FS function throws
  errors when the source file does not exist or the source file is too large.
- `copy_from_external_small`: Try to copy a file with small content into a file inside the TFS.
- `copy_from_external_large`: Try to copy a file with large content into a file inside the TFS.
- `test1`: Write and read to/from file with small string.
- `t1_2_1a_symlink_simple`: Check if a basic symlink creation is working.
- `t1_2_1b_hardlink_simple`: Check if a basic hardlink creatin is working.
- `t1_2_3_symlink_resolution_failure`: Check if a symlink stops working after deleting the original fine, and if it works again after restoring the original file.
- `t1_2_4_hardlink_indep`: Try to create a hardlink of a hardlink, multiple times.
- `t1_2_5_clear_data_blocks`: Test the TFS maximum data blocks limit.
- `t1_2_6_remove_file_create_new`: Create and write to a file. Delete the same file and check if a new with the same name is empty.
- `t1_2_7_create_hard_link_with_soft_link`: Try to create a hardlink from a softlink (can't happen).
- `t1_2_8_hard_link_unlinking`: Check if after creating and deleting a hardlink, the original file still has it's content.


## Student Made Tests

- `copy_from_external_extralarge`: Try to copy a file with extra large (maximum size) content into a file inside the TFS.
- `double_symlink`: Try to create a symlink from a symlink and check if it opens ok.
- `remove_open_file`: Check if removing an open file fails.
- `threads_create_multiple_files`: Create multiple files with the same path to check
if only one is created using multiple threads. After that use each file descriptor to write a single character (always appending to the end).
- `threads_multiple_writes`: Create and write to multiple files on different threads and check
expected results. 
- `threads_read_and_write`: Open a file and write to it on different threads. Each thread
should wait to acquire the lock and overwrite the previous one. Check the result with
multiple threads reading the file. 
- `threads_unlink_twice`: Attempts to unlink the same file from different threads.
One should return an error and the other should succeed. 
- `threads_write_tp_same_fd`: Uses multiple threads to write using the same file descriptor.
Test will write exactly 1024 bytes to the file, in the end, another call to tfs_write should return 0. 