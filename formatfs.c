#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "softwaredisk.h"

// Type for one inode.  Structure must be 32 bytes long.
typedef struct Inode {
  uint32_t size;       // file size in bytes
  uint16_t b[NUM_DIRECT_INODE_BLOCKS+1];  // direct blocks + 1
                                          // indirect block
} Inode;


// type for one indirect block.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).  Max
// file size calculation is based on use of uint16_t as type for block
// numbers.
typedef struct IndirectBlock {
  uint16_t b[NUM_SINGLE_INDIRECT_BLOCKS];
} IndirectBlock;


// type for block of Inodes. Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
typedef struct InodeBlock {
  Inode inodes[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)];
} InodeBlock;

// type for one directory entry.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
// Directory entry is free on disk if first character of filename is
// null.
typedef struct DirEntry {
  uint8_t file_is_open;    // file is open
  uint16_t inode_idx;      // inode #
  char name[MAX_FILENAME_SIZE];  // null terminated ASCII filename
} DirEntry;

// type for a one block bitmap.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
typedef struct FreeBitmap {
  uint8_t bytes[SOFTWARE_DISK_BLOCK_SIZE];
} FreeBitmap;

// main private file type
typedef struct FileInternals {
  uint32_t position;          // current file position
  FileMode mode;              // access mode for file
  Inode inode;                // cached inode
  DirEntry d;                 // cached dir entry
  uint16_t d_block;           // block # for cached dir entry
} FileInternals;

int main(){

    return 0;
}