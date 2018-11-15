#include <stdlib.h>
#include "oufs_lib.h"

#define debug 1

//TODO List:
//OUFILE* oufs_fopen(char *cwd, char *path, char *mode);
//void oufs_fclose(OUFILE *fp);
//int oufs_fwrite(OUFILE *fp, unsigned char * buf, int len);
//int oufs_fread(OUFILE *fp, unsigned char * buf, int len);
//int oufs_remove(char *cwd, char *path);
//int oufs_link(char *cwd, char *path_src, char *path_dst);

/**
 * Create a new file
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made, null will list info about cwd
 * @return 0 if successful, negative for failure
 *
 */
int oufs_touch(char *cwd, char *path) {
  OUFILE *fp = oufs_fopen(cwd, path, "a");
  oufs_fclose(fp);
  return 0;
}

/**
 * Opens a file for reading and writing
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made, null will list info about cwd
 * @param char * mode Either r, a, or w For read, write, append respectively
 * @return OUFILE * pointing to OUFILE struct if successful, NULL on failure
 *
 */
OUFILE* oufs_fopen(char *cwd, char *path, char *mode)
{

  INODE_REFERENCE child;
  INODE_REFERENCE parent;
  char local_name[FILE_NAME_SIZE];

  if (oufs_find_file(cwd, path, &parent, &child, local_name) != 0)
    return NULL;


  //case "r"
  if (!strcmp(mode, "r"))
  {
    if(debug)
    fprintf(stderr, "##Entered \"r\" mode for fopen\n");

    INODE inode;

    if (child == UNALLOCATED_INODE) {
      fprintf(stderr, "File doesn't exist, can't open for reading\n");
      return NULL;
    }
    else {
      oufs_read_inode_by_reference(child, &inode);
      if (inode.type != IT_FILE) { //Not a file! Exit!
        fprintf(stderr, "Path doesn't point to a file");
        return NULL;
      }
    }
    OUFILE *fp = malloc(sizeof(OUFILE));
    fp->inode_reference = child;
    fp->mode = 'r';
    fp->offset = 0;
    return fp;
  }

  //case "a"
  if (!strcmp(mode, "a")) {
    //check parent exists
    if (parent == UNALLOCATED_INODE) {
      fprintf(stderr, "Parent doesn't exist, invalid path\n");
      return NULL;
    }

    INODE inode;
    if (child == UNALLOCATED_INODE) {
      //Child doesn't exist, update inode, directory block, master block
      child = oufs_allocate_new_inode();
      if (child == UNALLOCATED_INODE) {
        fprintf(stderr, "No more inodes, can't create new file\n");
        return NULL;
      }

      //Update Directory block, if full exit (deallocating child)
      BLOCK d_block;
      oufs_read_inode_by_reference(parent, &inode);
      inode.size = inode.size + 1;
      if (inode.size > DIRECTORY_ENTRIES_PER_BLOCK) {
        //Too many directory entries
        fprintf(stderr, "No room in parent directory for new file\n");
        oufs_deallocate_old_inode(child);
        return NULL;
      }
      oufs_write_inode_by_reference(parent, &inode);

      vdisk_read_block(inode.data[0], &d_block);

      //Put in entry in first availible entry space
      int i;
      for (i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
      {
        if (strcmp(d_block.directory.entry[i].name, "") == 0) {//If entry is empty, write in, and break from for loop
          strcpy(d_block.directory.entry[i].name, local_name);
          d_block.directory.entry[i].inode_reference = child;
          vdisk_write_block(inode.data[0], &d_block);
          break;
        }
      }

      //Create a new inode for the new file
      oufs_clean_inode(&inode);
      inode.type = IT_FILE;
      inode.n_references = 1;
      inode.size = 0;

      //Write inode to disk by reference
      oufs_write_inode_by_reference(child, &inode);
    }
    else {
      oufs_read_inode_by_reference(child, &inode);
      if (inode.type != IT_FILE) { //Not a file! Exit!
        fprintf(stderr, "Path doesn't point to a file");
        return NULL;
      }
    }

    OUFILE *fp = malloc(sizeof(OUFILE));
    fp->inode_reference = child;
    fp->mode = 'a';
    fp->offset = inode.size;
    return fp;
  }

  //case "w"
  if (!strcmp(mode, "w")) {
    //Check parent exists
    if (parent == UNALLOCATED_INODE) {
      fprintf(stderr, "Parent doesn't exist, invalid path\n");
      return NULL;
    }

    //Check if child exists, if not create new file stuff
    INODE inode;
    if (child == UNALLOCATED_INODE) {
      //Child doesn't exist, update inode, directory block, master block
      child = oufs_allocate_new_inode();
      if (child == UNALLOCATED_INODE) {
        fprintf(stderr, "No more inodes, can't create new file\n");
        return NULL;
      }

      //Update Directory block, if full exit (deallocating child)
      BLOCK d_block;
      oufs_read_inode_by_reference(parent, &inode);
      inode.size = inode.size + 1;
      if (inode.size > DIRECTORY_ENTRIES_PER_BLOCK) {
        //Too many directory entries
        fprintf(stderr, "No room in parent directory for new file\n");
        oufs_deallocate_old_inode(child);
        return NULL;
      }
      oufs_write_inode_by_reference(parent, &inode);

      vdisk_read_block(inode.data[0], &d_block);

      //Put in entry in first availible entry space
      int i;
      for (i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
      {
        if (strcmp(d_block.directory.entry[i].name, "") == 0) {//If entry is empty, write in, and break from for loop
          strcpy(d_block.directory.entry[i].name, local_name);
          d_block.directory.entry[i].inode_reference = child;
          vdisk_write_block(inode.data[0], &d_block);
          break;
        }
      }

      //Create a new inode for the new file
      oufs_clean_inode(&inode);
      inode.type = IT_FILE;
      inode.n_references = 1;
      inode.size = 0;

      //Write inode to disk by reference
      oufs_write_inode_by_reference(child, &inode);
    }
    else { //If it does exist, deallocate all old blocks
      oufs_read_inode_by_reference(child, &inode);

      if (inode.type != IT_FILE) { //Not a file! Exit!
        fprintf(stderr, "Path doesn't point to a file");
        return NULL;
      }

      int i;
      for (i = 0; i < BLOCKS_PER_INODE; i++) {
        if (inode.data[i] != UNALLOCATED_BLOCK) {
          oufs_deallocate_old_block(inode.data[i]);
          inode.data[i] = UNALLOCATED_BLOCK;
        }
      }
      inode.size = 0;
      oufs_write_inode_by_reference(child, &inode);
    }

    OUFILE *fp = malloc(sizeof(OUFILE));
    fp->inode_reference = child;
    fp->mode = 'w';
    fp->offset = 0;
    return fp;
  }

  fprintf(stderr, "Incorrect fopen call, no mode\n");
  return NULL;
}
  /**
   * Closes a file pointer
   *
   * @param OUFILE * fp File pointer to be closed
   *
   */
void oufs_fclose(OUFILE *fp)
{
  free(fp);
}
