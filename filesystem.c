#include "filesystem.h"
#include "softwaredisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>

#define BITMAP_DISK_BLOCK 0
#define NUM_DIRECT_INODE_BLOCKS 12
#define NUM_SINGLE_INDIRECT_BLOCKS 256
#define MAX_FILENAME_SIZE 506
#define DIRECTORY_SIZE_LIMIT 50

/*
 HEY! IDIOT! WHEN YOU'RE DONE! CHANGE ALL explicit_bzero BACK TO bzero AND ALL string.h TO strings.h! DON'T FORGET, DUMBASS.
*/

//global cached variables//
int directory_size = 0;
int dir_cache_index = 0;
FSError fserror;

//easy compiling: gcc -o main filesystem.c softwaredisk.c

// Type for one inode.  Structure must be 32 bytes long.
typedef struct Inode
{
  uint32_t size;                           // file size in bytes
  uint16_t b[NUM_DIRECT_INODE_BLOCKS + 1]; // direct blocks + 1
                                           // indirect block
} Inode;

// type for one indirect block.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).  Max
// file size calculation is based on use of uint16_t as type for block
// numbers.
typedef struct IndirectBlock
{
  uint16_t b[NUM_SINGLE_INDIRECT_BLOCKS];
} IndirectBlock;

// type for block of Inodes. Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
typedef struct InodeBlock
{
  Inode inodes[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)];
} InodeBlock;

// type for one directory entry.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
// Directory entry is free on disk if first character of filename is
// null.
typedef struct DirEntry
{
  uint8_t file_is_open;         // file is open
  uint16_t inode_idx;           // inode #
  uint16_t disk_index;          //index of entry on the disk
  char name[MAX_FILENAME_SIZE]; // null terminated ASCII filename
} DirEntry;

DirEntry dir_cache[DIRECTORY_SIZE_LIMIT];

// type for a one block bitmap.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
typedef struct FreeBitmap
{
  uint8_t bytes[SOFTWARE_DISK_BLOCK_SIZE];
} FreeBitmap;

//delete this later
FreeBitmap mainMap;

// main private file type
typedef struct FileInternals
{
  uint32_t position; // current file position
  FileMode mode;     // access mode for file
  Inode inode;       // cached inode
  DirEntry d;        // cached dir entry
  uint16_t d_block;  // block # for cached dir entry
} FileInternals;

// open existing file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode)
{
  fserror = 0;
  DirEntry found_dir;
  for (int i = 0; i <= DIRECTORY_SIZE_LIMIT; i++)
  {
    if (!strcmp(dir_cache[i].name, name))
    {
      if (dir_cache[i].file_is_open == 1)
      {
        fserror = FS_FILE_OPEN;
        return NULL;
      }
      dir_cache[i].file_is_open = 1;
      found_dir = dir_cache[i];
      write_sd_block(&found_dir, found_dir.disk_index);
      break;
    }
    else if (i == DIRECTORY_SIZE_LIMIT)
    {
      fserror = FS_FILE_NOT_FOUND;
      return NULL;
    }
  }
  File opened;
  opened = malloc(sizeof(FileInternals));
  opened->d = found_dir;
  opened->position = 0;
  opened->mode = mode;
  //opened.d_block = ;
  //opened.inode = found_dir.inode_idx;
  return opened;
}
// find index of first zero bit in a disk block.  Returns -1 if no
// zero bit is found, otherwise returns the index of the zero bit.
int32_t find_zero_bit(uint8_t *data)
{
  int32_t i, j;
  int32_t freeidx = -1;

  for (i = 0; i < SOFTWARE_DISK_BLOCK_SIZE && freeidx < 0; i++)
  {
    for (j = 0; j <= 7 && freeidx < 0; j++)
    {
      if ((data[i] & (1 << (7 - j))) == 0)
      {
        freeidx = i * 8 + j;
      }
    }
  }

  return freeidx;
}

int returnZeroIndex(uint8_t byte)
{
  uint8_t mask = 1;
  for (uint8_t i = 0; i < 8; i++)
  {
    if (byte != (byte | (mask << i)))
    {
      return i;
    }
  }
  return -1;
}

// create and open new file with pathname 'name' and (implied) access
// mode READ_WRITE. The current file position is set at byte 0.
// Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name)
{
  fserror = 0;
  if (directory_size == DIRECTORY_SIZE_LIMIT)
  {
    fserror = FS_EXCEEDS_MAX_FILE_SIZE;
    return NULL;
  }
  int32_t i, j;
  int32_t index = -1;
  int32_t zeroIndex = -1;
  for (int i = 0; i < SOFTWARE_DISK_BLOCK_SIZE; i++)
  {
    zeroIndex = returnZeroIndex(mainMap.bytes[i]);
    if (zeroIndex >= 0)
    {
      index = (i * 8) + zeroIndex;
      break;
    }
  }
  mainMap.bytes[index / 8] |= 1 << (index % 8);

  //   for (i=0; i < SOFTWARE_DISK_BLOCK_SIZE && index < 0; i++) {
  //     for (j=0; j <= 7 && index < 0; j++) {
  //       if ((mainMap.bytes[i] & (1 << (7 - j))) == 0) {
  //         index = i * 8 + j;
  //
  //     }
  //     //mainMap.bytes[i] |= (1 << (index % 8));
  //   }
  // }
  //   //int index = find_zero_bit(mainMap.bytes);
  //   //printf("Current: %d\n", index);
  //   //mainMap.bytes[0] ^= 1UL << index;
  //   mainMap.bytes[1/8] |= 1 << (index%1);

  directory_size++;
  DirEntry dir;
  dir.file_is_open = 0;
  dir.disk_index = index;
  strcpy(dir.name, name);
  dir_cache[dir_cache_index] = dir;
  dir_cache_index++;
  write_sd_block(&dir, index);

  File created;
  created = malloc(sizeof(FileInternals));
  created->mode = READ_WRITE;
  created->position = 0;
  created->d = dir;
  created->d_block = index;

  return created;
}

// close 'file'.  Always sets 'fserror' global.
void close_file(File file)
{
  fserror = 0;
  DirEntry found_dir = file->d;
  if (found_dir.file_is_open == 0)
  {
    fserror = FS_FILE_NOT_OPEN;
    return;
  }
  printf("I am here! %d\n", file->d.disk_index);

  //This is probably the worst way to do this!
  for (int i = 0; i <= DIRECTORY_SIZE_LIMIT; i++)
  {
    if (!strcmp(dir_cache[i].name, found_dir.name))
    {
      dir_cache[i].file_is_open = 0;
      found_dir.file_is_open = 0;
    }
  }
  write_sd_block(&found_dir, found_dir.disk_index);
  printf("It's gone!\n");
  free(file);
}

// read at most 'numbytes' of data from 'file' into 'buf', starting at the
// current file position.  Returns the number of bytes read. If end of file is reached,
// then a return value less than 'numbytes' signals this condition. Always sets
// 'fserror' global.
unsigned long read_file(File file, void *buf, unsigned long numbytes)
{
}

// write 'numbytes' of data from 'buf' into 'file' at the current file position.
// Returns the number of bytes written. On an out of space error, the return value may be
// less than 'numbytes'.  Always sets 'fserror' global.
unsigned long write_file(File file, void *buf, unsigned long numbytes)
{
  unsigned long bytesWritten = 0;
  unsigned long bytesRemaining = numbytes;
  if (file->mode != READ_WRITE)
  {
    fserror = FS_FILE_READ_ONLY;
    return bytesWritten;
  }
  if (file->position >= sizeof(FileInternals))
  {
    fserror = FS_EXCEEDS_MAX_FILE_SIZE;
    return bytesWritten;
  }

  //if()

  while (bytesRemaining > 0 && file->position >= sizeof(FileInternals))
  {
  }

  return bytesWritten;
}

// sets current position in file to 'bytepos', always relative to the
// beginning of file.  Seeks past the current end of file should
// extend the file. Returns 1 on success and 0 on failure.  Always
// sets 'fserror' global.
int seek_file(File file, unsigned long bytepos)
{
}

// returns the current length of the file in bytes. Always sets 'fserror' global.
unsigned long file_length(File file)
{
}

// deletes the file named 'name', if it exists. Returns 1 on success, 0 on failure.
// Always sets 'fserror' global.
int delete_file(char *name)
{
}

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name)
{
}

// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void)
{
}
//main is in here for now just to make testing easy, we're gonna move to formatfs.c later

int main()
{
  init_software_disk();

  write_sd_block(&mainMap, BITMAP_DISK_BLOCK); //NOT SURE HOW TO USE THIS! BUT IT'S GOTTA GO IN BLOCK 5000!
  printf("This is the disk: %d\n", software_disk_size());
  printf("\n");
  File new = create_file("one");
  File new2 = create_file("two");
  File new3 = create_file("three");
  File new4 = create_file("four");
  close_file(new);

  File open3 = open_file("two", READ_WRITE);
  printf("This is a file I opened: %s\n", open3->d.name);
  close_file(open3);
  printf("\n Here's your indices! %d, %d, %d, %d", new->d_block, new2->d_block, new3->d_block, new4->d_block);
  return 0;
}