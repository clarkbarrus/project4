#include <stdlib.h>
#include "oufs_lib.h"

#define debug 0

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


  return 0;
}
