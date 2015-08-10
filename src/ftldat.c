/* The MIT License (MIT)

Copyright (c) 2015 Gabriel Corona

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#define _BSD_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <endian.h>

#include "ftldat.h"

static int process_file(FILE* file, uint32_t offset, int action)
{
  if (fseek(file, offset, SEEK_SET) == -1) {
    perror("seeking for file");
    return -1;
  }
  uint32_t data_size;
  uint32_t name_size;
  if (fread(&data_size, sizeof(data_size), 1, file) != 1) {
    fprintf(stderr, "Could not read data size count\n");
    return -1;
  }
  data_size = le32toh(data_size);
  if (fread(&name_size, sizeof(name_size), 1, file) != 1) {
    fprintf(stderr, "Could not read name size\n");
    return -1;
  }
  name_size = le32toh(name_size);
  char* name = malloc(name_size + 1);
  if (fread(name, 1, name_size, file) != name_size) {
    fprintf(stderr, "Could not read file name\n");
    goto free_name;
  }
  name[name_size] = '\0';

  void* data = NULL;
  FILE* output = NULL;
  switch(action) {
  case ACTION_LIST:
    printf("%s\n", name);
    free(name);
    break;
  case ACTION_EXTRACT:
    data = malloc(data_size);
    if (fread(data, 1, data_size, file) != data_size) {
      fprintf(stderr, "Could not read file data\n");
      goto free_data;
    }
    output = fopen(name, "wb");
    if (output == NULL) {
      perror("Opening output file");
      goto free_data;
    }
    if (fwrite(data, 1, data_size, output) != data_size) {
      fprintf(stderr, "Could not write file data\n");
      goto close_output;
    }
    fclose(output);
    free(data);
    free(name);
    return 0;
  default:
    fprintf(stderr, "Unexpected action\n");
    goto free_name;
  }

  return 0;

close_output:
  fclose(output);
free_data:
  free(data);
free_name:
  free(name);
  return -1;
}

static int process_archive(const char* file_name, int action)
{
  FILE* file = fopen(file_name, "rb");
  if (file == NULL) {
    perror("open archive file");
    return 1;
  }

  uint32_t slots_count;
  if (fread(&slots_count, sizeof(slots_count), 1, file) != 1) {
    fprintf(stderr, "Could not read slot count\n");
    goto close_file;
  }
  slots_count = le32toh(slots_count);

  uint32_t* slots = malloc(slots_count * sizeof(uint32_t));
  if (fread(slots, sizeof(uint32_t), slots_count, file) != slots_count) {
    fprintf(stderr, "Could not read slots\n");
    goto free_slots;
  }
  for (uint32_t i = 0; i != slots_count; ++i) {
    uint32_t offset = le32toh(slots[i]);
    if (offset == 0)
      continue;
    int res = process_file(file, offset, action);
    if (res == -1)
      goto free_slots;
  }

  return 0;

free_slots:
  free(slots);
close_file:
  fclose(file);
  return 1;
}

int main(int argc, char** argv)
{
  if (argc <= 1) {
    fprintf(stderr, "Bad arguments\n");
    return 1;
  }
  int mode = ACTION_NONE;
  for (char* p = argv[1]; *p != '\0'; ++p) {
    switch(*p) {
    case 'x':
      mode = ACTION_EXTRACT;
      break;
    case 't':
      mode = ACTION_LIST;
      break;
    default:
      fprintf(stderr, "Unknown option\n", *p);
      return 1;
    }
  }
  if (argc < 3) {
    fprintf(stderr, "Missing archive name\n");
    return 1;
  }
  switch(mode) {
  case ACTION_NONE:
    fprintf(stderr, "Missing action\n");
    return 1;
  case ACTION_EXTRACT:
  case ACTION_LIST:
      return process_archive(argv[2], mode);
  default:
    fprintf(stderr, "Unknown action\n");
    return 1;
  }
}
