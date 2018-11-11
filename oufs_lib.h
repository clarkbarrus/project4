#ifndef OUFS_LIB
#define OUFS_LIB
#include "oufs.h"

#define MAX_PATH_LENGTH 200

// PROVIDED
void oufs_get_environment(char *cwd, char *disk_name);

// PROJECT 3
int oufs_format_disk(char  *virtual_disk_name);
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode);
int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE *inode);
int oufs_find_file(char *cwd, char * path, INODE_REFERENCE *parent, INODE_REFERENCE *child, char *local_name); //TODO
int oufs_mkdir(char *cwd, char *path);
int oufs_list(char *cwd, char *path);
int oufs_rmdir(char *cwd, char *path);
int oufs_dir_entry_cmp(const void *entry_ref_1, const void *entry_ref_2);

// Helper functions in oufs_lib_support.c
void oufs_clean_directory_block(INODE_REFERENCE self, INODE_REFERENCE parent, BLOCK *block);
void oufs_clean_directory_entry(DIRECTORY_ENTRY *entry);
void oufs_clean_block(BLOCK *block);
void oufs_clean_inode(INODE *inode);
BLOCK_REFERENCE oufs_allocate_new_block();
void oufs_deallocate_old_block(BLOCK_REFERENCE old_block_reference);
INODE_REFERENCE oufs_allocate_new_inode();
void oufs_deallocate_old_inode(INODE_REFERENCE old_inode_reference);
int oufs_find_open_bit(unsigned char value);
INODE_REFERENCE oufs_find_entry(INODE *inode, char * entry_name);


// PROJECT 4 ONLY
OUFILE* oufs_fopen(char *cwd, char *path, char *mode);
void oufs_fclose(OUFILE *fp);
int oufs_fwrite(OUFILE *fp, unsigned char * buf, int len);
int oufs_fread(OUFILE *fp, unsigned char * buf, int len);
int oufs_remove(char *cwd, char *path);
int oufs_link(char *cwd, char *path_src, char *path_dst);

#endif
