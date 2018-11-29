#include <stdlib.h>
#include "oufs_lib.h"

#define debug 0

/**
 * Read the ZPWD and ZDISK environment variables & copy their values into cwd and disk_name.
 * If these environment variables are not set, then reasonable defaults are given.
 *
 * @param cwd String buffer in which to place the OUFS current working directory.
 * @param disk_name String buffer containing the file name of the virtual disk.
 */
void oufs_get_environment(char *cwd, char *disk_name)
{
	// Current working directory for the OUFS
	char *str = getenv("ZPWD");
	if(str == NULL) {
		// Provide default
		strcpy(cwd, "/");
	}else{
		// Exists
		strncpy(cwd, str, MAX_PATH_LENGTH-1);
	}

	// Virtual disk location
	str = getenv("ZDISK");
	if(str == NULL) {
		// Default
		strcpy(disk_name, "vdisk1");
	}else{
		// Exists: copy
		strncpy(disk_name, str, MAX_PATH_LENGTH-1);
	}

}

/**
 * Configure a directory entry so that it has no name and no inode
 *
 * @param entry The directory entry to be cleaned
 */
void oufs_clean_directory_entry(DIRECTORY_ENTRY *entry)
{
	entry->name[0] = 0;  // No name
	entry->inode_reference = UNALLOCATED_INODE;
}

/**
 * Configure a directory entry so that it has no name and no inode
 *
 * @param entry The directory entry to be cleaned
 */
void oufs_clean_inode(INODE *inode)
{
	//Initialize and empty inode
	inode->type = IT_NONE;  // No name
	inode->n_references = 0;
	inode->size = 0;
	int i;
	for (i = 0; i < BLOCKS_PER_INODE; i++)
	{
		inode->data[i] = UNALLOCATED_BLOCK;
	}
}

/**
 * Initialize a directory block as an empty directory
 *
 * @param self Inode reference index for this directory
 * @param self Inode reference index for the parent directory
 * @param block The block containing the directory contents
 *
 */
void oufs_clean_directory_block(INODE_REFERENCE self, INODE_REFERENCE parent, BLOCK *block)
{
	// Debugging output
	if(debug)
		fprintf(stderr, "##New clean directory: self=%d, parent=%d\n", self, parent);

	// Create an empty directory entry
	DIRECTORY_ENTRY entry;
	oufs_clean_directory_entry(&entry);

	// Copy empty directory entries across the entire directory list
	for(int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
		block->directory.entry[i] = entry;
	}

	// Now we will set up the two fixed directory entries

	// Self
	strncpy(entry.name, ".", 2);
	entry.inode_reference = self;
	block->directory.entry[0] = entry;

	// Parent (same as self
	strncpy(entry.name, "..", 3);
	entry.inode_reference = parent;
	block->directory.entry[1] = entry;

}

/**
 * Sets block to all 0 bytes
 */
void oufs_clean_block(BLOCK *block)
{
	//Write the block to be all 0's
	int j;
	for(j = 0; j < BLOCK_SIZE; j++)
	{
		block->data.data[j] = (unsigned char) 0x0;
	}
}

/**
 * Allocate a new data block
 *
 * If one is found, then the corresponding bit in the block allocation table is set
 *
 * @return The index of the allocated data block.  If no blocks are available,
 * then UNALLOCATED_BLOCK is returned
 *
 */
BLOCK_REFERENCE oufs_allocate_new_block()
{
	BLOCK block;
	// Read the master block
	vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

	// Scan for an available block
	int block_byte;
	int flag;

	// Loop over each byte in the allocation table.
	for(block_byte = 0, flag = 1; flag && block_byte < N_BLOCKS_IN_DISK / 8; ++block_byte) {
		if(block.master.block_allocated_flag[block_byte] != 0xff) {
			// Found a byte that has an opening: stop scanning
			flag = 0;
			break;
		};
	};
	// Did we find a candidate byte in the table?
	if(flag == 1) {
		// No
		if(debug)
			fprintf(stderr, "No blocks\n");
		return(UNALLOCATED_BLOCK);
	}

	// Found an available data block

	// Set the block allocated bit
	// Find the FIRST bit in the byte that is 0 (we scan in bit order: 0 ... 7)
	int block_bit = oufs_find_open_bit(block.master.block_allocated_flag[block_byte]);

	// Now set the bit in the allocation table
	block.master.block_allocated_flag[block_byte] |= (1 << block_bit);

	// Write out the updated master block
	vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

	if(debug)
		fprintf(stderr, "Allocating block=%d (%d)\n", block_byte, block_bit);

	// Compute the block index
	BLOCK_REFERENCE block_reference = (block_byte << 3) + block_bit;

	if(debug)
		fprintf(stderr, "Allocating block=%d\n", block_reference);

	// Done
	return(block_reference);
}

/**
 * Deallocate an old block
 *
 * @param BLOCK_REFERENCE Reference to the block to be deallocated in the master block
 *
 */
void oufs_deallocate_old_block(BLOCK_REFERENCE old_block_reference)
{
	BLOCK block;
	// Read the master block
	vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

	int block_byte = old_block_reference/8;
	int block_bit = old_block_reference%8;

	// Now set the bit in the allocation table
	block.master.block_allocated_flag[block_byte] &= (~(1 << block_bit));

	// Write out the updated master block
	vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

	if(debug)
		fprintf(stderr, "Deallocating block=%d (%d)\n", block_byte, block_bit);
}

/**
 * Allocate a new inode entry
 *
 * If one is found, then the corresponding bit in the inode allocation table is set
 *
 * @return The index of the allocated inode entry.  If no inode entries are available,
 * then UNALLOCATED_INODE is returned
 *
 */
INODE_REFERENCE oufs_allocate_new_inode()
{
	BLOCK block;
	// Read the master block
	vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

	// Scan for an available block
	int inode_byte;
	int flag;

	// Loop over each byte in the allocation table.
	for(inode_byte = 0, flag = 1; flag && inode_byte < N_INODES / 8; ++inode_byte) {
		if(block.master.inode_allocated_flag[inode_byte] != 0xff) {
			// Found a byte that has an opening: stop scanning
			flag = 0;
			break;
		};
	};
	// Did we find a candidate byte in the table?
	if(flag == 1) {
		// No
		if(debug)
			fprintf(stderr, "No inodes\n");
		return(UNALLOCATED_INODE);
	}

	// Found an available data inode entry

	// Set the block allocated bit
	// Find the FIRST bit in the byte that is 0 (we scan in bit order: 0 ... 7)
	int inode_bit = oufs_find_open_bit(block.master.inode_allocated_flag[inode_byte]);

	// Now set the bit in the allocation table
	block.master.inode_allocated_flag[inode_byte] |= (1 << inode_bit);

	// Write out the updated master block
	vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

	if(debug)
		fprintf(stderr, "Allocating inode=%d (%d)\n", inode_byte, inode_bit);

	// Compute the inode index
	INODE_REFERENCE inode_reference = (inode_byte << 3) + inode_bit;

	if(debug)
		fprintf(stderr, "Allocating inode=%d\n", inode_reference);

	// Done
	return(inode_reference);
}

/**
 * Deallocate an old inode
 *
 * @param INODE_REFERENCE Reference to the inode to be deallocated in the master block
 *
 */
void oufs_deallocate_old_inode(INODE_REFERENCE old_inode_reference)
{
	BLOCK block;
	// Read the master block
	vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

	int inode_byte = old_inode_reference/8;
	int inode_bit = old_inode_reference%8;

	// Now set the bit in the allocation table
	block.master.inode_allocated_flag[inode_byte] &= (~(1 << inode_bit));

	// Write out the updated master block
	vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

	if(debug)
		fprintf(stderr, "Deallocating inode=%d (%d)\n", inode_byte, inode_bit);
}

/**
 * Format the virtual disk
 *
 * @param char * virtual_disk_name takes the name of the virtual disk to be formatted
 *
 * @return Success or failure, 0 or -1 respectively
 *
 */
int oufs_format_disk(char  *virtual_disk_name)
{
	if(debug)
	fprintf(stderr, "Formatting disk: %s \n", virtual_disk_name);

	//Zero out the whole disk
	BLOCK_REFERENCE i;
	BLOCK block;
	oufs_clean_block(&block); //Write the block to be all 0's

	for(i = 0; i < N_BLOCKS_IN_DISK; i++)
	{
		//Write block back to disk
		vdisk_write_block(i, &block);
	}

	//Initialize master blocks, first 10 block allocated, first inode allocated
	//Block Allocated Table: 1111 1111 1100 0000 ....
	//Inode Allocated Table 1000 0000 0000 ....
	block.master.block_allocated_flag[0] = 0xff;
	block.master.block_allocated_flag[1] = 0x3;
	block.master.inode_allocated_flag[0] = 0x1;
	vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

	//Initialize first inode
	INODE inode;
	oufs_clean_inode(&inode);
	inode.type = IT_DIRECTORY;
	inode.n_references = 1;
	inode.data[0] = ROOT_DIRECTORY_BLOCK; //Point inode to first nonmaster/noninode block
	inode.size = 2; //Include . and .. directories to be added

	oufs_write_inode_by_reference(MASTER_BLOCK_REFERENCE, &inode);

	//Initialize all other inodes to unallocated inodes

	oufs_clean_inode(&inode);
	for (i = 1; i < N_INODES; i++)
	{
		oufs_write_inode_by_reference(i, &inode);
	}


	//Initialize root directory block with . and ..
	//Use oufs_clean_directory_block()

	oufs_clean_directory_block(0, 0, &block);
	vdisk_write_block(ROOT_DIRECTORY_BLOCK, &block); //Initialize the root directory

	return 0;
}

/**
 *  Given an inode reference, read the inode from the virtual disk.
 *
 *  @param i Inode reference (index into the inode list)
 *  @param inode Pointer to an inode memory structure.  This structure will be
 *                filled in before return)
 *  @return 0 = successfully loaded the inode
 *         -1 = an error has occurred
 *
 */
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
	if(debug)
		fprintf(stderr, "##Fetching inode %d\n", i);

	// Find the address of the inode block and the inode within the block
	BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
	int element = (i % INODES_PER_BLOCK);

	BLOCK b;
	if(vdisk_read_block(block, &b) == 0) {
		// Successfully loaded the block: copy just this inode
		*inode = b.inodes.inode[element];
		return(0);
	}
	// Error case
	return(-1);
}

/**
 *  Given an inode reference, write the inode to the virtual disk.
 *
 *  @param i Inode reference (index into the inode list)
 *  @param inode Pointer to an inode memory structure.  This structure will be
 *                filled written to the disk)
 *  @return 0 = successfully loaded the inode
 *         -1 = an error has occurred
 *
 */
int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
	if(debug)
		fprintf(stderr, "##Writing to inode %d\n", i);

	// Find the address of the inode block and the inode within the block
	BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
	int element = (i % INODES_PER_BLOCK);

	BLOCK b;
	if(vdisk_read_block(block, &b) == 0) {
		// Successfully loaded the block: only change specified inode

		b.inodes.inode[element] = *inode; //Copied inode to block

		//Write block back to disk
		if(vdisk_write_block(block, &b) != 0)
		{
			return(-1);
		}

		return(0);
	}
	// Error case
	return(-1);
}

/**
 *  Given an unsigned char (1 byte), return the index of the first 0 bit
 *
 *  @param value Byte to find open bit in
 *
 *  @return Index 0-7 indicating open bytes
Return -1 on error
 *
 */
int oufs_find_open_bit(unsigned char value)
{
	//xxxx xxxx
	int i;
	for (i = 0; i < 8; i++)
	{
		if (!(value & (1 << i)))
		{
			return i; //Found a 0 bit
		}
	}

	fprintf(stderr, "Error: oufs_find_open_bit() failed\n");
	return -1; //All bits are 0
}

/**
 * Create a new directory
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made
 * @return 0 if successful, -1 if find file fails, -2 if oufs is full
 * then UNALLOCATED_BLOCK is returned
 *
 */
int oufs_mkdir(char *cwd, char *path)
{
	if(debug)
	fprintf(stderr,"##Making directory, cwd: %s, path: %s\n", cwd, path);

	//Find inode that path is pointing to from cwd. Path may be absolute or relative or invalid
	//Path will include the directory to be created
	INODE_REFERENCE parent;
	INODE_REFERENCE child;
	char local_name[FILE_NAME_SIZE];

	if (oufs_find_file(cwd, path, &parent, &child, local_name) != 0)
	return -1;

	//Check that parent inode exists, check that path doesn't already exist

	if (parent == UNALLOCATED_INODE) {//Invalid file path
		fprintf(stderr, "Invalid file path, parent does not exist\n");
		return -2;
	}

	if (child != UNALLOCATED_INODE) { //File already exists,
		fprintf(stderr, "File already exists, cannot make directory\n");
		return -3;
	}

	//Find next available directory block
	//Update master block for new directory block for new file
	BLOCK_REFERENCE new_dir_block = oufs_allocate_new_block();

	if (new_dir_block == UNALLOCATED_BLOCK) {//No more blocks in fs
		fprintf(stderr, "No more room in OUFS for new blocks\n");
		return -4;
	}

	INODE_REFERENCE inode_ref = oufs_allocate_new_inode();
	if (inode_ref == UNALLOCATED_INODE) //No more inodes in fs
	{
		fprintf(stderr, "No more room in OUFS for new inodes\n");

		//Unallocate Block that has already been allocated
		oufs_deallocate_old_block(new_dir_block);
		return -5;
	}

	//Update parent inode
	INODE inode;
	oufs_read_inode_by_reference(parent, &inode);
	inode.size = inode.size + 1;
	if (inode.size > DIRECTORY_ENTRIES_PER_BLOCK) { //No more room in parent directory
		fprintf(stderr, "Error: No more room in parent directory for more entries\n");
		oufs_deallocate_old_block(new_dir_block);
		oufs_deallocate_old_inode(inode_ref);
		return -6;
	}
	oufs_write_inode_by_reference(parent, &inode);

	BLOCK block;
	//Update parent directory block
	vdisk_read_block(inode.data[0], &block);
	//Put in entry in first availible entry space
	int i;
	for (i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
	{
		if (strcmp(block.directory.entry[i].name, "") == 0) {//If entry is empty, write in, and break from for loop
			strcpy(block.directory.entry[i].name, local_name);
			block.directory.entry[i].inode_reference = inode_ref;
			vdisk_write_block(inode.data[0], &block);
			break;
		}
	}

	//Create a new inode for the new directory
	oufs_clean_inode(&inode);
	inode.type = IT_DIRECTORY;
	inode.n_references = 1;
	inode.size = 2;
	inode.data[0] = new_dir_block;

	//Write inode to disk by reference
	oufs_write_inode_by_reference(inode_ref, &inode);

	//Initialize the new directory block
	oufs_clean_directory_block(inode_ref, parent, &block);
	//Write directory block to disk
	vdisk_write_block(new_dir_block, &block);

	return 0;
}

/**
 * Remove an old directory
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made
 * @return 0 if successful, negative for failure
 *
 */
int oufs_rmdir(char *cwd, char *path)
{
	if(debug)
	fprintf(stderr,"##Removing directory, cwd: %s, path: %s\n", cwd, path);

	//Find inode that path is pointing to from cwd. Path may be absolute or relative or invalid
	//Path will include the directory to be created
	INODE_REFERENCE parent;
	INODE_REFERENCE child;
	char local_name[FILE_NAME_SIZE];

	if (oufs_find_file(cwd, path, &parent, &child, local_name) != 0)
	return -1;

	//Check that parent inode exists, check that path doesn't already exist

	if ((strcmp(local_name, ".") == 0) || (strcmp(local_name, "..") == 0)) {
		fprintf(stderr, "Error: \".\" and \"..\" may not be removed\n");
		return -2;
	}

	if (strcmp(local_name, "/") == 0) {
		fprintf(stderr, "Error: \"/\" may not be removed\n");
		return -3;
	}

	if (parent == UNALLOCATED_INODE) {//Invalid file path
		fprintf(stderr, "Invalid file path, parent does not exist\n");
		return -4;
	}

	if (child == UNALLOCATED_INODE) { //File already exists,
		fprintf(stderr, "Directory doesn't exist, can't remove\n");
		return -5;
	}

	//Check that directory is empty
	BLOCK block;
	INODE inode;
	oufs_read_inode_by_reference(child, &inode);

	//Check that block is a directory
	if (inode.type != IT_DIRECTORY) {
		fprintf(stderr, "Error: %s must be a directory\n", local_name);
		return -6;
	}

	//Check that directory is empty
	if (inode.size > 2)
	{
		//Inode is not empty
		fprintf(stderr, "Directory is not empty, can't remove\n");
		return -7;
	}


	BLOCK_REFERENCE old_dir_block = inode.data[0];

	oufs_clean_block(&block); //Write the block to be all 0's
	vdisk_write_block(old_dir_block, &block);
	oufs_deallocate_old_block(old_dir_block);

	//Update inode block
	oufs_clean_inode(&inode);
	oufs_write_inode_by_reference(child, &inode);
	oufs_deallocate_old_inode(child);


	//Update parent directory (clear entry and inode pointer)
	// and inode (decrement size)

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
 * List a info about a file or about cwd if path is null
 *
 * @param char * cwd Current working directory of OUFS
 * @param char * path Path of the new directory to be made, null will list info about cwd
 * @return 0 if successful, negative for failure
 *
 */
int oufs_list(char *cwd, char *path)
{
	if(debug)
	fprintf(stderr,"##oufs_list, cwd: %s, path: %s\n", cwd, path);

	//Find inode that path is pointing to from cwd. Path may be absolute or relative or invalid
	INODE_REFERENCE parent;
	INODE_REFERENCE child;
	char local_name[FILE_NAME_SIZE];

	if (path == NULL) { //path is NULL, list info about cwd instead of path
		oufs_find_file(cwd, cwd, &parent, &child, local_name);
	}
	else { //List info about path
		if (oufs_find_file(cwd, path, &parent, &child, local_name) != 0)
		return -1;
	}

	//parent, child, and local_name have all of the info about
	//the file we want to list more info about

	//Check that file exists
	if (child == UNALLOCATED_INODE) { //File doesn't exist, error
		fprintf(stderr, "Error: file does not exist\n");
		return -2;
	}

	INODE inode;
	BLOCK block;
	DIRECTORY_ENTRY d_entry;
	int i;
	//Fetch inode
	oufs_read_inode_by_reference(child, &inode);

	//If it is a file list its name
	if (inode.type == IT_FILE) {
		//Go to parent inode, go to parent dblock, get entry name that points to child
		oufs_read_inode_by_reference(parent, &inode);
		vdisk_read_block(inode.data[0], &block);

		for(i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {
			if (block.directory.entry[i].inode_reference == child) { //Found child directory entry
				fprintf(stdout, "%s\n", block.directory.entry[i].name);
				break;
			}
		}

		return 0;
	}

	//Fetch child dblock in order to list contents
	vdisk_read_block(inode.data[0], &block);

	//Since file is a directory, list valid entries in ASCII order, with newlines and / at the end if entry is a directory
	//Sort the array contained in block.directory.entry which is a DIRECTORY_ENTRY *

	qsort(block.directory.entry, DIRECTORY_ENTRIES_PER_BLOCK, sizeof(d_entry), oufs_dir_entry_cmp);

	//Block now has entries sorted in ASCII order. Lets print them.

	for(i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
		if(block.directory.entry[i].inode_reference != UNALLOCATED_INODE) {
			printf("%s", block.directory.entry[i].name);
			oufs_read_inode_by_reference(block.directory.entry[i].inode_reference, &inode);
			if (inode.type == IT_DIRECTORY) {
				printf("/");
			}
			printf("\n");
		}
	}

	return 0;
}

/**
 * Compares two entries passed by reference
 *
 * @param const void *entry_ref_1 First dir entry
 * @param const void *entry_ref_2 Second dir entry
 *
 * @return an integer less than, equal to,
       or greater than zero if the first entry is
       respectively less than, equal to, or greater than the second
 */
int oufs_dir_entry_cmp(const void *entry_ref_1, const void *entry_ref_2)
{
	return strcmp(((*((DIRECTORY_ENTRY*)entry_ref_1)).name), ((*((DIRECTORY_ENTRY*)entry_ref_2)).name));
}

/**
 * Given current working directory and a path returns info about the file pointed to by path
 *
 * @param char *cwd Current working directory of OUFS
 * @param char *path path pointing to file of interest
 * @param INODE_REFERENCE *parent Reference to store inode location of parent of file of interest
 * @param INODE_REFERENCE *child Reference to store inode location of file of interest
 * @param char *local_name Reference to store the name of the file of interest
 *
 * @return 0 if successful
 *	parent will be the inode reference to the second to last directory in
 *		the path if it exists
 *	child will be the reference to to that last directory in the path if
 *		it exists
 *	local_name will contain the name of the last file given in path
 *
 *	If a file in the path does not exist the inode_reference will be set to
 *	UNALLOCATED_INODE
 *
 *	-1 if path contains a file that is not a directory
 */
int oufs_find_file(char *cwd, char * path, INODE_REFERENCE *parent, INODE_REFERENCE *child, char *local_name)
{
	if(debug)
	fprintf(stderr, "##Finding file from cwd: %s, to %s\n", cwd, path);

	INODE_REFERENCE cur_inode[2] = {UNALLOCATED_INODE, UNALLOCATED_INODE};
	char * cur_file_name[2];
	//If path doesn't start from / find starting inode
	//Else starting inode is inode 0, root directory
	if ((*path) == '/') { //path is absolute
		cur_inode[0] = 0;
		cur_file_name[1] = "/";

		if(debug)
		{
			fprintf(stderr, "##%s is absolute\n", path);
			fprintf(stderr, "##cur_inode: %d cur_file_name: %s\n", cur_inode[0], cur_file_name[1]);
		}
	}
	else { //Path is relative
		if(debug)
		fprintf(stderr, "##%s is relative\n", path);

		//Need to determine starting inode from cwd
		char buffer_string[FILE_NAME_SIZE];
		oufs_find_file(cwd, cwd, &(cur_inode[1]), &(cur_inode[0]), buffer_string);
		cur_file_name[1] = buffer_string;
		if(debug)
		{
			fprintf(stderr, "##For cwd: %s we found...\n", cwd);
			fprintf(stderr, "###cur_inode: %d cur_file_name: %s\n", cur_inode[0], cur_file_name[1]);
		}
	}

	//Parse path i.e. foo/bar or /baz/foo/bar
	//Use parsed path to trace to the child cur_inode[0], keeping track of parent in cur_inode[1]
	cur_file_name[0]= strtok(path, "/");

	//If empty, leave cur_inode alone. Else find inode for d_entry local_name

	while (cur_file_name[0]!= NULL) //While there are still tokens
	{
		if(debug)
		fprintf(stderr, "##Seeking the inode for file: %s, from cur_inode: %d\n", cur_file_name[0], cur_inode[0]);

		cur_inode[1] = cur_inode[0];
		if (cur_inode[1] == UNALLOCATED_INODE) {
			//Do nothing
			if(debug)
			fprintf(stderr, "##Directory doesn't exist, continue parsing path\n");
		}
		else { //cur_inode[1] is a valid inode

			INODE inode;
			//Inode for directory block
			oufs_read_inode_by_reference(cur_inode[1], &inode);

			if (inode.type == IT_DIRECTORY) { //Check that inode[1] refers to a directory block
				cur_inode[0] = oufs_find_entry(&inode, cur_file_name[0]);
			}
			else {
				fprintf(stderr, "Error, invalid file path: %s\n", path);
				return -1;
			}
			if (debug)
			fprintf(stderr, "##For name: %s we found inode: %d \n###in directory with name: %s and inode: %d\n", cur_file_name[0], cur_inode[0], cur_file_name[1], cur_inode[1]);
		}

		cur_file_name[1] = cur_file_name[0];
		cur_file_name[0]= strtok(NULL, "/");
	}

	if(debug)
	fprintf(stderr, "##Done parsing path, returning cur_inode: %d, inode's parent: %d, local_file_name: %s\n", cur_inode[0], cur_inode[1], cur_file_name[1]);

	(*parent) = cur_inode[1];
	(*child) = cur_inode[0];
	strcpy(local_name, cur_file_name[1]);

	return 0;
}

/**
 * Given current an inode reference and an entry name, checks if entry is in the directory
 *
 * @param char *cwd Current working directory of OUFS
 * @param char *path path pointing to file of interest
 * @param INODE_REFERENCE *parent Reference to store inode location of parent of file of interest
 * @param INODE_REFERENCE *child Reference to store inode location of file of interest
 * @param char *local_name Reference to store the name of the file of interest
 *
 * @return INODE_REFERENCE of entry if found, or UNALLOCATED_INODE if there is no such entry
 *
 *
 */
INODE_REFERENCE oufs_find_entry(INODE *inode, char *entry_name)
{
	if(debug)
	fprintf(stderr, "##Looking for entry %s in with inode pointing to block %d\n", entry_name, inode->data[0]);

	BLOCK block;

	//Read directory block
	vdisk_read_block(inode->data[0], &block);

	//Search each directory entry checking names against entry_name
	//When found return inode reference pointed to by entry
	//Else, return UNALLOCATED_INODE

	int i;
	for (i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) { //Check each directory entry
		if (strcmp(entry_name, block.directory.entry[i].name) == 0) {
			//Match!
			if (debug)
			fprintf(stderr, "##Found entry %s\n", entry_name);
			return block.directory.entry[i].inode_reference;
		}
	}

	if (debug)
	fprintf(stderr, "##Entry not found\n");
	//Found no matches in the directory
	return UNALLOCATED_INODE;
}
