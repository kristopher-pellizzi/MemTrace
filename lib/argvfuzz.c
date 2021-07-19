/*
   american fuzzy lop++ - LD_PRELOAD for fuzzing argv in binaries
   ------------------------------------------------------------

   Copyright 2019-2020 Kjell Braden <afflux@pentabarf.de>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

#define _GNU_SOURCE                                        /* for RTLD_NEXT */
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "argv-fuzz-inl.h"

int __libc_start_main(int (*main)(int, char **, char **), int argc, char **argv,
                      void (*init)(void), void (*fini)(void),
                      void (*rtld_fini)(void), void *stack_end) {

  int (*orig)(int (*main)(int, char **, char **), int argc, char **argv,
              void (*init)(void), void (*fini)(void), void (*rtld_fini)(void),
              void *stack_end);
  int    sub_argc;
  char **sub_argv;

  (void)argc;
  (void)argv;

  orig = dlsym(RTLD_NEXT, __func__);

  if (!orig) {

    fprintf(stderr, "hook did not find original %s: %s\n", __func__, dlerror());
    exit(EXIT_FAILURE);

  }

  int fd = 0;

  // If the program in analysis uses an input file, replace fd 0 with the fd of the input file itself.
  // The tracer launcher should be responsible to prepare it so that it also contains the data which will 
  // be used as arguments and to set the required environment variable
  char* inputIndices = getenv("INPUT_FILE_ARGV_INDICES");
  new_file_path = getenv("FUZZ_INSTANCE_NAME");
  if(new_file_path == NULL){
    exit(EXIT_FAILURE);
  }
  char* inputFilePath;
  unsigned long index;

  char* ptr = inputIndices;
  char digits[] = "0123456789";
  unsigned indices_num = 16;
  unsigned long* indices = (unsigned long*) malloc(sizeof(unsigned long) * indices_num);
  unsigned counter = 0;

  if(inputIndices != NULL){
    sscanf(inputIndices, "%lu", &index);
    inputFilePath = argv[index];
    fd = open(inputFilePath, O_RDONLY);
    if(fd == -1){
      fprintf(stderr, "Cannot open %s\n", inputFilePath);
      fprintf(stderr, "%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    do{
      if(counter >= indices_num){
        indices_num *= 2;
        indices = (unsigned long*) realloc(indices, sizeof(unsigned long) * indices_num);
      }
      indices[counter++] = index;
      size_t len = strspn(ptr, digits);
      ptr += len;
      len = strcspn(ptr, digits);
      ptr += len;
      sscanf(ptr, "%lu", &index);
    } while(*ptr != '\x00');
  }

  sub_argv = afl_init_argv(&sub_argc, fd);

  if(sub_argc <= 0){
    // We must have at least the executable name
    sub_argc = 1;
  }

  for(unsigned i = 0; i < counter; ++i){
    index = indices[i];
    if(index < sub_argc){
      sub_argv[index] = new_file_path;
    }
  }

  free(indices);

  sub_argv[0] = argv[0];

  // We copied the part of the input file which must be read by the application inside a new file
  // whose path is defined by new_file_path. In order to make the application read it correctly, we need
  // to overwrite the initial input file with the new one where the initial part (used to define arguments)
  // has been cut out.
  // rename(new_file_path, inputFilePath);

  // If the fuzzer is used with an input file, it is possible to specify a path to a file to be used as stdin.
  // This might be useful if the program in analysis reads from both the input file and stdin and we want to
  // fuzz the input file. Note that the stdin file will remain contant.
  // If this is not used, the stdin will be empty, and every read will return an empty string.
  char* stdinFilePath = getenv("STDIN_FILE");
  if(inputIndices != NULL && stdinFilePath != NULL){
    int stdin_fd = open(stdinFilePath, O_RDONLY);
    dup2(stdin_fd, 0);
  }

  fprintf(stderr, "\n\nRunning original __libc_start_main\n");

  return orig(main, sub_argc, sub_argv, init, fini, rtld_fini, stack_end);

}

