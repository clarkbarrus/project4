/**
Format a new disk for the oufs file system

CS3113

*/

#include <stdio.h>
#include <string.h>

#include "oufs_lib.h"

#define debug 1

int main(int argc, char** argv) {

	// Open the virtual disk
	// Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);

  vdisk_disk_open(disk_name);

	oufs_format_disk(disk_name);

	// Clean up
	vdisk_disk_close();

	return 0;
}
