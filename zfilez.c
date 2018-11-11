/**
List all files in a directory in the OU file system
CS3113

*/

#include <stdio.h>
#include <string.h>

#include "oufs_lib.h"

int main(int argc, char** argv) {
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);

  // Open the virtual disk
  vdisk_disk_open(disk_name);

  // Check arguments
  if(argc == 2) {
    // List info about specified path
    oufs_list(cwd, argv[1]);

  }else{
    // No file specified, list files in cwd instead
    oufs_list(cwd, NULL);
  }

  // Clean up
  vdisk_disk_close();


}
