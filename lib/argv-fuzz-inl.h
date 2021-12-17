/*
   american fuzzy lop++ - sample argv fuzzing wrapper
   ------------------------------------------------

   Originally written by Michal Zalewski

   Copyright 2015 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This file shows a simple way to fuzz command-line parameters with stock
   afl-fuzz. To use, add:

   #include "/path/to/argv-fuzz-inl.h"

   ...to the file containing main(), ideally placing it after all the
   standard includes. Next, put AFL_INIT_ARGV(); near the very beginning of
   main().

   This will cause the program to read NUL-delimited input from stdin and
   put it in argv[]. Two subsequent NULs terminate the array. Empty
   params are encoded as a lone 0x02. Lone 0x02 can't be generated, but
   that shouldn't matter in real life.

   If you would like to always preserve argv[0], use this instead:
   AFL_INIT_SET0("prog_name");

*/

#ifndef _HAVE_ARGV_FUZZ_INL
#define _HAVE_ARGV_FUZZ_INL

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#define AFL_INIT_ARGV()          \
  do {                           \
                                 \
    argv = afl_init_argv(&argc); \
                                 \
  } while (0)

#define AFL_INIT_SET0(_p)        \
  do {                           \
                                 \
    argv = afl_init_argv(&argc); \
    argv[0] = (_p);              \
    if (!argc) argc = 1;         \
                                 \
  } while (0)

#define MAX_CMDLINE_LEN 100000
#define MAX_CMDLINE_PAR 50000

char* new_file_path;

static char **afl_init_argv(char* executable, int *argc, int fd) {

  static char  in_buf[MAX_CMDLINE_LEN];
  static char *ret[MAX_CMDLINE_PAR];

  char *ptr = in_buf;
  int   rc = 1;
  ssize_t readBytes;

  if ((readBytes = read(fd, in_buf, MAX_CMDLINE_LEN)) == -1) {
    fprintf(stderr, "Cannot read from file descriptor %d\n", fd);
    exit(EXIT_FAILURE);
  }

  // Set executable path as argument in argv[0]
  ret[0] = executable;

  // If input files begins with '\x00', check next character. If it's not a '\x00', store the empty argument as a first argument
  // and increase the ptr; otherwise (second character is a '\x00' as well) there are no arguments, so just increase the ptr, so that
  // everything after the double '\x00' will be considered as input
  if(!*ptr){
    if(*(ptr + 1)){
      ret[rc++] = ptr;
    }
    ++ptr;
  }

  while (*ptr && rc < MAX_CMDLINE_PAR) {

    ret[rc] = ptr;
    if (ret[rc][0] == 0x02 && !ret[rc][1]) ret[rc]++;
    rc++;

    while (*ptr){
      ptr++;

      if(ptr >= in_buf + MAX_CMDLINE_LEN){
        *(ptr - 1) = '\x00';
        *(ptr - 2) = '\x00';
        ptr --;
        break;
      }

      if(ptr >= in_buf + readBytes){
        ptr = in_buf + readBytes;
        *ptr = '\x00';
        --ptr;
        break;
      }
    }
    ptr++;

    // If we already read all the bytes read from fd, there can't be more arguments
    if(ptr >= in_buf + readBytes){
      ptr = in_buf + readBytes;
      --ptr; // Required because ptr is increased again outside the loop
      break;
    }

  }

  ptr++;
  off_t diff = ptr - in_buf;

  if(fd == 0){
    off_t res = lseek(fd, diff, SEEK_SET);
    if(res != diff){
      fprintf(stderr, "Can't seek properly inside the input file for stdin\n");
      fprintf(stderr, "%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  else{
    FILE* f = fopen(new_file_path, "wb");
    fwrite(ptr, 1, readBytes - diff, f);
    if(ferror(f)){
      fprintf(stderr, "Error while writing to new_file\n");
      exit(EXIT_FAILURE);
    }

    while(readBytes != 0){
      readBytes = read(fd, in_buf, MAX_CMDLINE_LEN);
      fwrite(in_buf, 1, readBytes, f);
    }

    fclose(f);
    close(fd);
  }

  *argc = rc;

  return ret;

}

#undef MAX_CMDLINE_LEN
#undef MAX_CMDLINE_PAR

#endif                                              /* !_HAVE_ARGV_FUZZ_INL */

