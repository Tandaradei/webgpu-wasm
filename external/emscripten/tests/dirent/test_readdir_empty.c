/*
 * Copyright 2014 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>


int main(int argc, char** argv) {
  if (mkdir("/tmp", S_IRWXG) != 0 && errno != EEXIST) {
    printf("Unable to create dir '/tmp'\n");
    return -1;
  }

  if (mkdir("/tmp/1", S_IRWXG) != 0 && errno != EEXIST) {
    printf("Unable to create dir '/tmp/1'\n");
    return -1;
  }

  if (mkdir("/tmp/1/", S_IRWXG) != 0 && errno != EEXIST) {
    printf("Unable to create dir '/tmp/1/'\n");
    return -1;
  }

  DIR *dir = opendir("/tmp");

  if (!dir) {
    printf("Unable to open dir '/tmp'\n");
    return -2;
  }

  struct dirent *dirent;

  while ((dirent = readdir(dir)) != 0) {
    printf("Found '%s'\n", dirent->d_name);

    if (strlen(dirent->d_name) == 0) {
      printf("Found empty path!\n");
      return -3;
    }
  }

  closedir(dir);

  printf("success\n"); 

#ifdef REPORT_RESULT
  REPORT_RESULT(0);
#endif
  return 0;
}

