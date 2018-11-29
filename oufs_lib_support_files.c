#include <stdlib.h>
#include <stdio.h>
#include "oufs_lib.h"

#define debug 0
static int BUFFER_SIZE = BLOCK_SIZE;

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
 * Create a new file from stdin
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made, null will list info about cwd
 * @return 0 if successful, negative for failure
 *
 */
int oufs_create(char *cwd, char *path) {
  OUFILE *fp = oufs_fopen(cwd, path, "w");
  if (fp == NULL)
    return -1;

  //Read from stdin into a buffer, and fwrite to disk until fwrite stops or EOF
  unsigned char buf[BUFFER_SIZE];
  int len = fread(buf, 1, BUFFER_SIZE, stdin);
  int ret = 0;
  while (len != 0) {
    if (debug) {
      fprintf(stderr, "##Calling fwrite on input length %d\n", len);
    }

    ret = oufs_fwrite(fp, buf, len);
    if (ret != len) { //File isn't writing anymore
      break;
    }

    fp->offset = fp->offset + ret;
    len = fread(buf, 1, BLOCK_SIZE, stdin);
  }

  oufs_fclose(fp);
  return 0;
}

/**
 * Append to a file from stdin
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made, null will list info about cwd
 * @return 0 if successful, negative for failure
 *
 */
int oufs_append(char *cwd, char *path) {
  OUFILE *fp = oufs_fopen(cwd, path, "a");
  if (fp == NULL)
    return -1;

  //Read from stdin into a buffer, and fwrite to disk until fwrite stops or EOF
  unsigned char buf[BUFFER_SIZE];
  int len = fread(buf, 1, BUFFER_SIZE, stdin);
  int ret = 0;
  while (len != 0) {
    if (debug) {
      fprintf(stderr, "##Calling fwrite on input length %d\n", len);
    }

    ret = oufs_fwrite(fp, buf, len);
    if (ret != len) { //File isn't writing anymore
      break;
    }

    fp->offset = fp->offset + ret;
    len = fread(buf, 1, BLOCK_SIZE, stdin);
  }

  oufs_fclose(fp);
  return 0;
}

/**
 * Read a file to stdout
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made, null will list info about cwd
 * @return 0 if successful, negative for failure
 *
 */
int oufs_more(char *cwd, char *path) {
  OUFILE *fp = oufs_fopen(cwd, path, "r");
  if (fp == NULL)
    return -1;

  //Use a buffer to read from the file, stop when file is empty
  unsigned char buf[BUFFER_SIZE + 1];
  int ret = oufs_fread(fp, buf, BUFFER_SIZE);
  buf[ret] = '\0';
  fprintf(stdout, "%s", buf);

  while (ret != 0) {
    fp->offset = fp->offset + ret; //Update offset after read of first 256 bytes
    ret = oufs_fread(fp, buf, BUFFER_SIZE);
    buf[ret] = '\0';
    fprintf(stdout, "%s", buf);
  }

  fprintf(stdout, "\n");
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
OUFILE* oufs_fopen(char *cwd, char *path, char *mode) {

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
void oufs_fclose(OUFILE *fp) {
  free(fp);
}

/**
 * Writes from a buffer to a file
 *
 * @param OUFILE *fp file pointer for file of interest
 * @param unsigned char *buf Buffer containing contents to be written to file
 * @param int len size of buf
 * @return int Number of bytes written to file, -1 if error
 *
 */
int oufs_fwrite(OUFILE *fp, unsigned char * buf, int len) {
  if (fp == NULL) {
    fprintf(stderr, "Invalid file pointer\n");
    return -1;
  }

  if (!(fp->mode == 'a' || fp->mode == 'w')) {
    fprintf(stderr, "Invalid file pointer mode\n");
    return -1;
  }

  INODE inode;
  oufs_read_inode_by_reference(fp->inode_reference, &inode);

  //Set up variables for file copy

  //Index of block that we are writing to
  int block_index = fp->offset / BLOCK_SIZE;
  //Offset inside of the block that we are writing to
  int block_offset = fp->offset % BLOCK_SIZE;
  //Offset inside the buffer we are writing from
  int buffer_offset = 0;
  //Amount that has been written so far
  int write_count = 0;
  //Amount that should be copied to the current file block
  int copy_amount = MIN(BLOCK_SIZE - block_offset, len - buffer_offset);
  BLOCK block;
  BLOCK_REFERENCE block_reference;

  if (debug) { //Debugging info about variables
    fprintf(stderr, "##(Before loop)Writing to block data[%d] == %d at offset %d, %d bytes.\n##From buffer at offset %d. We have written %d so far.\n", block_index, inode.data[block_index], block_offset, copy_amount, buffer_offset, write_count);
  }

  //Continue while there is still data that should/can be copied
  while (copy_amount > 0) {

    //Check if block is already allocated, allocate new one if not
    if (block_index >= BLOCKS_PER_INODE) { //File full, no more data blocks
      if (debug) {
        fprintf(stderr, "##No more blocks for inode, ending fwrite\n");
      }
      break;
    }
    else if (inode.data[block_index] == UNALLOCATED_BLOCK) {
      //Allocate new data block
      block_reference = oufs_allocate_new_block();
      if (block_reference == UNALLOCATED_BLOCK) {
        if (debug) {
          fprintf(stderr, "##No more blocks in file system, ending fwrite\n");
        }
        break;
      }
      inode.data[block_index] = block_reference;
    }
    else {
      block_reference = inode.data[block_index];
    }

    if (debug) { //Debugging info about variables
      fprintf(stderr, "##(In loop)Writing to block data[%d] == %d at offset %d, %d bytes.\n##From buffer at offset %d. We have written %d so far.\n", block_index, inode.data[block_index], block_offset, copy_amount, buffer_offset, write_count);
    }

    //Get block for writing
    vdisk_read_block(block_reference, &block);

    //Write from block_offset to either end of block or end of buf
    for (int i = block_offset; i < (block_offset + copy_amount); i++) {
      block.data.data[i] = buf[i + buffer_offset];
    }

    vdisk_write_block(block_reference, &block);

    //Update write_count, block_offset, buffer_offset, block_index, copy_amount
    block_index++;
    block_offset = 0;
    buffer_offset = buffer_offset + copy_amount;
    write_count = write_count + copy_amount;
    //Amount that should be copied to the current file block
    copy_amount = MIN(BLOCK_SIZE, len - buffer_offset);
  }

  if (debug) { //Debugging info about variables
    fprintf(stderr, "##(Out of loop)Writing to block data[%d] == %d at offset %d, %d bytes.\n##From buffer at offset %d. We have written %d so far.\n", block_index, inode.data[block_index], block_offset, copy_amount, buffer_offset, write_count);
  }

  //Set inode size ot be num bytes written plus size of inode
  inode.size = inode.size + write_count;
  oufs_write_inode_by_reference(fp->inode_reference, &inode);

  //Return num bytes written
  return write_count;
}

/**
 * Writes from a buffer to a file
 *
 * @param OUFILE *fp file pointer for file of interest
 * @param unsigned char *buf Buffer containing contents to be written to file
 * @param int len size of buf
 * @return int Number of bytes written to file, -1 if error
 *
 */
int oufs_fread(OUFILE *fp, unsigned char * buf, int len) {

  if (debug) {
    fprintf(stderr, "fread called, fp->offset %d, fp->inode_reference %d.\n", fp->offset, fp->inode_reference);
  }

  if (fp == NULL) {
    fprintf(stderr, "Invalid file pointer\n");
    return -1;
  }

  if (!(fp->mode == 'r')) {
    fprintf(stderr, "Invalid file pointer mode\n");
    return -1;
  }

  INODE inode;
  oufs_read_inode_by_reference(fp->inode_reference, &inode);

  //Set up variables for file read

  //Index of block that we are writing to
  int block_index = fp->offset / BLOCK_SIZE;
  //Offset inside of the block that we are writing to
  int block_offset = fp->offset % BLOCK_SIZE;
  //Offset inside the buffer we are writing from
  int buffer_offset = 0;
  //Amount that has been written so far
  int read_count = 0;
  //Amount that should be copied from the current file block
  //Minimized between, block size, remainder of buffer, and remainder of file
  int read_amount = MIN(BLOCK_SIZE - block_offset, len - buffer_offset);
  read_amount = MIN(read_amount, inode.size - fp->offset);

  if (debug) {
    fprintf(stderr, "##Calculating read_amount: min(BLOCK_SIZE - block_offset: %d, len - buffer_offset: %d, inode.size - fp->offset: %d\n)", BLOCK_SIZE - block_offset, len - buffer_offset, inode.size - fp->offset);
  }

  BLOCK block;
  BLOCK_REFERENCE block_reference;

  if (debug) { //Debugging info about variables
    fprintf(stderr, "##(Before loop)Reading from block data[%d] == %d at offset %d, %d bytes.\n##From buffer at offset %d. We have read %d so far.\n", block_index, inode.data[block_index], block_offset, read_amount, buffer_offset, read_count);
  }

  //Continue while there is still data that should/can be copied
  while (read_amount > 0) {

    block_reference = inode.data[block_index];

    if (debug) { //Debugging info about variables
      fprintf(stderr, "##(In loop)Reading from block data[%d] == %d at offset %d, %d bytes.\n##From buffer at offset %d. We have read %d so far.\n", block_index, inode.data[block_index], block_offset, read_amount, buffer_offset, read_count);
    }

    //Get block for reading
    vdisk_read_block(block_reference, &block);

    //Read from block_offset to either end of block or end of buf
    for (int i = block_offset; i < (block_offset + read_amount); i++) {
      buf[i + buffer_offset] = block.data.data[i];
    }

    //Update write_count, block_offset, buffer_offset, block_index, copy_amount
    block_index++;
    block_offset = 0;
    buffer_offset = buffer_offset + read_amount;
    read_count = read_count + read_amount;
    //Amount that should be copied from the current file block
    read_amount = MIN(BLOCK_SIZE, len - buffer_offset);
    read_amount = MIN(read_amount, inode.size - fp->offset - read_count);
  }

  if (debug) { //Debugging info about variables
    fprintf(stderr, "##(Out of loop)Reading from block data[%d] == %d at offset %d, %d bytes.\n##From buffer at offset %d. We have read %d so far.\n", block_index, inode.data[block_index], block_offset, read_amount, buffer_offset, read_count);
  }

  //Return num bytes written
  return read_count;
}

/**
 * Removes specified file
 *
 * @param char *cwd Current working directory
 * @param char *path Path to the file of interest
 *
 * @return int 0 on success, -1 if error
 */
int oufs_remove(char *cwd, char *path) {
  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char local_name[MAX_PATH_LENGTH];
  if (0 > oufs_find_file(cwd, path, &parent, &child, local_name)) {
    return -1;
  }

  if(child == UNALLOCATED_INODE) {
    fprintf(stderr, "File does not exist, can't remove \n");
    return -1;
  }

  INODE inode;
  BLOCK block;
  oufs_read_inode_by_reference(child, &inode);

  if(inode.type != IT_FILE) {
    fprintf(stderr, "File does is not a file, can't remove \n");
    return -1;
  }

  inode.n_references--;
  //Decrement n_references, and delete inode if n_references is 0
  if (inode.n_references == 0) {
    //Delete Inode, and deallocate data blocks
    oufs_clean_inode(&inode);
    oufs_write_inode_by_reference(child, &inode);
    oufs_deallocate_old_inode(child);
  }
  else { //Update n_references in inode
    oufs_write_inode_by_reference(child, &inode);
  }

  //Delete d_entry from parent


  //Update parent directory (clear entry and inode pointer)
  //and inode (decrement size)

  //Update parent inode
  oufs_read_inode_by_reference(parent, &inode);
  inode.size = inode.size - 1;
  oufs_write_inode_by_reference(parent, &inode);

  //Update parent directory block
  vdisk_read_block(inode.data[0], &block);
  //Remove directory entry for removed file
  int i;
  for (i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
  {
    if (strcmp(block.directory.entry[i].name, local_name) == 0) {//If entry is the removed file, overwrite entry, and break from for loop
      oufs_clean_directory_entry(&(block.directory.entry[i]));
      vdisk_write_block(inode.data[0], &block);
      break;
    }
  }

  return 0;
}

/**
 * Links an existing file to a new file
 *
 * @param char *cwd Current working directory
 * @param char *path Path to the file of interest
 *
 * @return int 0 on success, -1 if error
 */
 //int oufs_link(char *cwd, char *path_src, char *path_dst);
