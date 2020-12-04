#include "filesystem.h"
#include "softwaredisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>


//limitation constants placed on the system
#define NUM_DIRECT_INODE_BLOCKS 12
#define NUM_SINGLE_INDIRECT_BLOCKS 256
#define MAX_FILENAME_SIZE 498
#define DIRECTORY_SIZE_LIMIT 50
#define MAX_FILE_SIZE 134144

//global cached variables
int directory_size = 0;
int dir_cache_index = 0;
FSError fserror;

//given structures, modified as needed

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
  uint32_t entry_length;        //total byte length of entry's data
  char name[MAX_FILENAME_SIZE]; // null terminated ASCII filename
} DirEntry;

DirEntry dir_cache[DIRECTORY_SIZE_LIMIT];

// type for a one block bitmap.  Structure must be
// SOFTWARE_DISK_BLOCK_SIZE bytes (software disk block size).
typedef struct FreeBitmap
{
  uint8_t bytes[SOFTWARE_DISK_BLOCK_SIZE];
} FreeBitmap;

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

  //search the directory cache to find the desired file, return errors if open or non existent.
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
  
  //create the File structure to return at the end of the function
  File opened;
  opened = malloc(sizeof(FileInternals));
  opened->d = found_dir;
  opened->position = 0;
  opened->mode = mode;
  opened->d_block = found_dir.disk_index;
  Inode inode;
  int ret = read_sd_block(&inode, found_dir.inode_idx);
  opened->inode = inode;
  return opened;
}
// find index of first zero bit in a disk block.  Returns -1 if no
// zero bit is found, otherwise returns the index of the zero bit.


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

int32_t find_zero_bit(uint8_t *data)
{
  int32_t i, j;
  int32_t index = -1;
  int32_t zeroIndex = -1; 
  for (int i = 0; i < SOFTWARE_DISK_BLOCK_SIZE; i++)
    {
      zeroIndex = returnZeroIndex(data[i]);
      if (zeroIndex >= 0)
      {
        index = (i * 8) + zeroIndex;
        break;
      }
    }

  return index;
}

// create and open new file with pathname 'name' and (implied) access
// mode READ_WRITE. The current file position is set at byte 0.
// Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name)
{

  //check for error cases initially
  fserror = 0;
  if (directory_size == DIRECTORY_SIZE_LIMIT)
  {
    fserror = FS_EXCEEDS_MAX_FILE_SIZE;
    return NULL;
  }
  if (file_exists(name)){
    fserror = FS_FILE_ALREADY_EXISTS;
    return NULL;
  }
  if(!strcmp(name, "") || !strcmp(name, "\0")){
    fserror = FS_ILLEGAL_FILENAME;
    return NULL;
  }

  //find zero bits and flip them
  int32_t index = find_zero_bit(mainMap.bytes);
  mainMap.bytes[index / 8] |= 1 << (index % 8);
  int32_t inode_index = find_zero_bit(mainMap.bytes);
  mainMap.bytes[inode_index / 8] |= 1 << (inode_index % 8);

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

  //increase the directory size, set the necessary variables and store the entry
  directory_size++;
  DirEntry dir;
  dir.file_is_open = 0;
  dir.disk_index = index;
  dir.inode_idx = inode_index;
  strcpy(dir.name, name);
  dir_cache[dir_cache_index] = dir;
  dir_cache_index++;
  write_sd_block(&dir, index);

  //store the inode immediately next to the entry
  Inode inode;
  write_sd_block(&inode, inode_index);


  //create the necessary file
  File created;
  created = malloc(sizeof(FileInternals));
  created->mode = READ_WRITE;
  created->position = 0;
  created->d = dir;
  created->d_block = index;
  created->d.entry_length = 0;
  created->inode = inode;

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
  //this is most likely inefficient, but find the file to be closed by name and set its open status to false
  for (int i = 0; i <= DIRECTORY_SIZE_LIMIT; i++)
  {
    if (!strcmp(dir_cache[i].name, found_dir.name))
    {
      dir_cache[i].file_is_open = 0;
      found_dir.file_is_open = 0;
    }
  }
  //free the file variable's memory
  write_sd_block(&found_dir, found_dir.disk_index);
  free(file);
}

// read at most 'numbytes' of data from 'file' into 'buf', starting at the
// current file position.  Returns the number of bytes read. If end of file is reached,
// then a return value less than 'numbytes' signals this condition. Always sets
// 'fserror' global.
unsigned long read_file(File file, void *buf, unsigned long numbytes)
{
  fserror = 0;
  //no time to implement this, here is our pseudocode.
  /*
    find position
    find which block of data we start in
    read that block and go to start position in the data
    begin reading into buffer out of that block for as long as numbytes
    if end of block is reached, open the next block using the inode indices
    read until end of file is reached or the length of buffer is equal to num bytes
    return the amount of bytes read.

  */
  return 0;
}

// write 'numbytes' of data from 'buf' into 'file' at the current file position.
// Returns the number of bytes written. On an out of space error, the return value may be
// less than 'numbytes'.  Always sets 'fserror' global.

unsigned long write_file(File file, void *buf, unsigned long numbytes)
{
  fserror = 0;
  Inode inode = file->inode;
  //[1 2 3 4 5 0 0 0 0 0 0]
  //step 1: get file position
  //step 2: figure out which index it goes to
  //step 3: see if block at that index is full/half full
  //step 4: if full, create new block at next available spot on disc and store index in inode
  //if not full, write at it until full then create new block if needed
  //done!
   
  unsigned long bytesWritten = 0;
  unsigned long bytesRemaining = numbytes;
  unsigned long position = (file->position) % 512;
  unsigned long blockIndex = (long) ceil((file-> position) / 512);
  char* block = malloc(sizeof(char)* 512);

/* if (reamining bytees > bytes in bvlck){
    unsigned long new block indeex = find the fucking zero 
    flip the burch
    inde array ++ 
    write the block love letterrs
}
*/
  if (file->mode != READ_WRITE)
  {
    fserror = FS_FILE_READ_ONLY;
    return bytesWritten;
  }
  if (file->position >= MAX_FILE_SIZE)
  {
    fserror = FS_EXCEEDS_MAX_FILE_SIZE;
    return bytesWritten;
  }
  if(file->d_block == 5000){
      fserror = FS_EXCEEDS_MAX_FILE_SIZE;
      return bytesWritten;
    }
  //if inode array is empty, then find an index which is empty and store it in inode array.
  if(inode.b[0] == 0){
      unsigned long requiredBlocks = ceil(numbytes / 512);
      for(long i = 0; i < requiredBlocks; i++){
          if(i >= 12){
            printf("We never implemented indirect! Yikes!\n");
            break;
          }
          uint32_t index = find_zero_bit(mainMap.bytes);
          mainMap.bytes[index / 8] |= 1 << (index % 8);
          inode.b[i] = index;

      } 
  }

  while(bytesRemaining > 0 && file->position < MAX_FILE_SIZE){

    unsigned long bytesWrittenThisRun = 0;
    
    if(bytesRemaining < 512 || position != 0){
      read_sd_block(block, inode.b[blockIndex]);
      unsigned long bytesLeftInBlock = 512 - position;
      unsigned long tempBytes = 0;
      
      if(bytesRemaining > bytesLeftInBlock){
        for(int i = 0; i < 13; i++){
          if(inode.b[i] == 0){
            uint32_t index = find_zero_bit(mainMap.bytes);
            mainMap.bytes[index / 8] |= 1 << (index % 8);
            inode.b[i] = index;
            break;
          }
        }
      }

      if(bytesRemaining < bytesLeftInBlock){
        memcpy((block + position), buf, bytesRemaining);
        tempBytes = bytesRemaining;
        write_sd_block(block, inode.b[blockIndex]);
      }
      else{
        memcpy((block + position), buf, bytesLeftInBlock);
        tempBytes = bytesLeftInBlock;
        write_sd_block(block, inode.b[blockIndex]);
        blockIndex++;
        
      }
      
      bytesWrittenThisRun = tempBytes;
    }
    else{
      if(bytesRemaining == 512){
        memcpy(block, buf, 512);
        write_sd_block(block, inode.b[blockIndex]);
        bytesWrittenThisRun = 512;
      }
      else{
        for(int i = 0; i < 13; i++){
          if(inode.b[i] == 0){
            uint32_t index = find_zero_bit(mainMap.bytes);
            mainMap.bytes[index / 8] |= 1 << (index % 8);
            inode.b[i] = index;
            break;
          }
        }
        memcpy(block, buf, 512);
        write_sd_block(block, inode.b[blockIndex]);
        bytesWrittenThisRun = 512;
        blockIndex++;

      }
      
    }

    

    bytesWritten = bytesWritten + bytesWrittenThisRun;
    bytesRemaining = bytesRemaining - bytesWrittenThisRun;
    file->d.entry_length = file->d.entry_length + bytesWrittenThisRun;
    position = 0;
  }

  return bytesWritten;
}

// sets current position in file to 'bytepos', always relative to the
// beginning of file.  Seeks past the current end of file should
// extend the file. Returns 1 on success and 0 on failure.  Always
// sets 'fserror' global.
int seek_file(File file, unsigned long bytepos)
{
    fserror = 0;
    if(bytepos > file->d.entry_length){
      if(bytepos >= MAX_FILE_SIZE){
        fserror = FS_EXCEEDS_MAX_FILE_SIZE;
        return 0;
      }
      //TO DO add more blocks to file
    }
    file->position = bytepos;
    return 1;
}

// returns the current length of the file in bytes. Always sets 'fserror' global.
unsigned long file_length(File file)
{
  fserror = 0;
  if(!file_exists(file->d.name)){
    return 0;
  }
  return file->d.entry_length;

}

// deletes the file named 'name', if it exists. Returns 1 on success, 0 on failure.
// Always sets 'fserror' global.
int delete_file(char *name)
{
  fserror = 0;
  DirEntry found_dir;
  for (int i = 0; i <= DIRECTORY_SIZE_LIMIT; i++){
    if(!strcmp(dir_cache[i].name, name)){
      found_dir = dir_cache[i];
      //does this work?
      bzero(&dir_cache, i);
    }
    else{
      fserror = FS_FILE_NOT_FOUND;
      return 0;
    }
  }
  if(found_dir.file_is_open == 1){
    fserror = FS_FILE_OPEN;
    return 0;
  }
  //delete file
  return 1;
}

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name)
{
  fserror = 0;
  for(int i = 0; i <= DIRECTORY_SIZE_LIMIT; i++){
    if(!strcmp(dir_cache[i].name, name)){
      return 1;
    }
    else{
      fserror = FS_FILE_NOT_FOUND;
      return 0;
    }
  }
}

// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void) {
	switch (fserror) {
		case FS_NONE:
			printf("FS: No error.\n");
			break;
		case FS_OUT_OF_SPACE:
			printf("FS: The operation caused the software disk to fill up\n");
			break;
		case FS_FILE_NOT_OPEN:
			printf("FS: Attempted read/write/close/etc. on file that isn't open.\n");
			break;
		case FS_FILE_OPEN:
			printf("FS: The file is already open. Concurrent opens are not supported and neither is deleting a file that is open.\n");
			break;
		case FS_FILE_NOT_FOUND:
			printf("FS: Attempted open or delete of file that does not exist.\n");
			break;
		case FS_FILE_READ_ONLY:
			printf("FS: Attempted write of file opened for READ_ONLY.\n");
			break;
		case FS_FILE_ALREADY_EXISTS:
			printf("FS: Attempted creation of file with existing name.\n");
			break;
		case FS_EXCEEDS_MAX_FILE_SIZE:
			printf("FS: Seek or write would exceed max file size.\n");
			break;
		case FS_ILLEGAL_FILENAME:
			printf("FS: Filename begins with a null character.\n");
			break;
		case FS_IO_ERROR:
			printf("FS: Something really bad happened.\n");
			break;
		default:
			printf("FS: Unknown error code %d.\n", fserror);
  }
}
//main is in here for now just to make testing easy, we're gonna move to formatfs.c later

int main()
{
  init_software_disk();

  write_sd_block(&mainMap, 5000); //NOT SURE HOW TO USE THIS! BUT IT'S GOTTA GO IN BLOCK 5000!
  printf("This is the disk: %d\n", software_disk_size());
  DirEntry testDir;
  printf("Size: %d\n", sizeof(testDir));
  printf("\n");
  File new = create_file("one");
  File new2 = create_file("two");
  File new3 = create_file("three");
  File new4 = create_file("four");
  close_file(new);

  File open3 = open_file("two", READ_WRITE);
  fs_print_error();
  printf("This is a file I opened: %s\n", open3->d.name);
  int ret = write_file(open3, "123456", 6);
  close_file(open3);
  fs_print_error();
  printf("\n Here's your indices! %d, %d, %d, %d", new->d_block, new2->d_block, new3->d_block, new4->d_block);
  return 0;
}