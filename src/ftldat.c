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
#include <string.h>
#include <endian.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ftldat.h"

static int create_directory_for(const char* name)
{
  char* path = strdup(name);
  for (char* p = path; *p != '\0'; ++p) {
    if (*p == '/') {
      *p = '\0';
      mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
      *p = '/';
    }
  }
  free(path);
  return 0;
}

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

  if (action | ACTION_LIST)
    printf("%s\n", name);

  if (action | ACTION_EXTRACT) {
    data = malloc(data_size);
    if (fread(data, 1, data_size, file) != data_size) {
      fprintf(stderr, "Could not read file data\n");
      goto free_data;
    }
    create_directory_for(name);
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
  }
  free(name);
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

static int create_archive(const char* archive_name, int file_count, char** files)
{
  FILE* output = fopen(archive_name, "wb");
  if (output == NULL) {
    perror("Opening target archive file");
    return 1;
  }

  uint32_t slots_count = 0xc68;
  if (file_count > slots_count)
    slots_count = file_count;
  uint32_t temp_slot_count = htole32(slots_count);
  if (fwrite(&temp_slot_count, sizeof(uint32_t), 1, output) != 1) {
    fprintf(stderr, "Could not write slot count\n");
    goto close_output;
  }

  uint32_t* slots = calloc(slots_count, sizeof(uint32_t));
  if (fwrite(slots, sizeof(uint32_t), slots_count, output) != slots_count) {
    fprintf(stderr, "Could not (temporary) slots\n");
    goto close_output;
  }

  for (int i = 0; i != file_count; ++i) {
    const char* file_name = files[i];

    long offset = ftell(output);
    if (offset == -1) {
      fprintf(stderr, "Could not get offset for file %s\n", file_name);
      goto free_slots;
    }
    slots[i] = htole32(offset);

    FILE* file = fopen(file_name, "rb");
    if (file == NULL) {
      fprintf(stderr, "Could not open file %s\n", file_name);
      fclose(file);
      goto free_slots;
    }

    // Stat:
    int fd = fileno(file);
    if (fd == -1) {
      fprintf(stderr, "Could not get size of file %s", file_name);
      fclose(file);
      goto free_slots;
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
      fprintf(stderr, "Could not get size of file %s", file_name);
      fclose(file);
      goto free_slots;
    }
    uint32_t data_size =file_stat.st_size;
    uint32_t temp_data_size = htole32(data_size);
    if (fwrite(&temp_data_size, sizeof(uint32_t), 1, output) != 1) {
      fprintf(stderr, "Could not write file data size for %s\n", file_name);
      fclose(file);
      goto close_output;
    }

    uint32_t name_size = strlen(file_name);
    uint32_t temp_name_size = htole32(name_size);
    if (fwrite(&temp_name_size, sizeof(uint32_t), 1, output) != 1) {
      fprintf(stderr, "Could not write file name size for %s\n", file_name);
      fclose(file);
      goto close_output;
    }

    if (fwrite(file_name, sizeof(char), name_size, output) != name_size) {
      fprintf(stderr, "Could not write file name for %s\n", file_name);
      fclose(file);
      goto close_output;
    }

    char* data = malloc(data_size);
    if (fread(data, sizeof(char), data_size, file) != data_size) {
      fprintf(stderr, "Could not read data from file %s\n", file_name);
      free(data);
      fclose(file);
      goto close_output;
    }
    if (fwrite(data, sizeof(char), data_size, output) != data_size) {
      fprintf(stderr, "Could not write file data for %s\n", file_name);
      free(data);
      fclose(file);
      goto close_output;
    }

    free(data);
    fclose(file);
  }

  if (fseek(output, sizeof(uint32_t), SEEK_SET) == -1) {
    fprintf(stderr, "Could not rewind for writing finale offsets\n");
    goto free_slots;
  }
  if (fwrite(slots, sizeof(uint32_t), slots_count, output) != slots_count) {
    fprintf(stderr, "Could not (final) slots\n");
    goto free_slots;
  }
  free(slots);
  fclose(output);

  return 0;
free_slots:
  free(slots);
close_output:
  fclose(output);
  return 1;
}

int main(int argc, char** argv)
{
  if (argc <= 1) {
    fprintf(stderr, "Bad arguments\n");
    return 1;
  }
  int action = ACTION_NONE;
  for (char* p = argv[1]; *p != '\0'; ++p) {
    switch(*p) {
    case 'x':
      action |= ACTION_EXTRACT;
      break;
    case 't':
      action |= ACTION_LIST;
      break;
    case 'c':
      action |= ACTION_CREATE;
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
  if (action == ACTION_NONE) {
    fprintf(stderr, "Missing action\n");
    return 1;
  } else if (action & ACTION_EXTRACT || action & ACTION_LIST) {
    return process_archive(argv[2], action);
  } else if (action & ACTION_CREATE) {
    return create_archive(argv[2], argc - 3, argv + 3);
  } else {
    fprintf(stderr, "Unknown action\n");
    return 1;
  }
}
