#include "filesystem.h"
#include "softwaredisk.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>

#define BITMAP_DISK_BLOCK 0
#define NUM_DIRECT_INODE_BLOCKS 12
#define NUM_SINGLE_INDIRECT_BLOCKS 256
#define MAX_FILENAME_SIZE 508
#define DIRECTORY_SIZE_LIMIT 50

/*
 HEY! IDIOT! WHEN YOU'RE DONE! CHANGE ALL explicit_bzero BACK TO bzero AND ALL string.h TO strings.h! DON'T FORGET, DUMBASS.
*/

//global cached variables//
int directory_size = 0;
FSError fserror;

//easy compiling: gcc -o main filesystem.c softwaredisk.c

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

//delete this later
FreeBitmap mainMap;

// main private file type
typedef struct FileInternals {
  uint32_t position;          // current file position
  FileMode mode;              // access mode for file
  Inode inode;                // cached inode
  DirEntry d;                 // cached dir entry
  uint16_t d_block;           // block # for cached dir entry
} FileInternals;

// open existing file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode){
 
}

// create and open new file with pathname 'name' and (implied) access
// mode READ_WRITE. The current file position is set at byte 0.
// Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name){
    if(directory_size == DIRECTORY_SIZE_LIMIT){
        fserror = FS_EXCEEDS_MAX_FILE_SIZE;
        return NULL;
    }
    int index = -1;
    for (int i = 0; i <= SOFTWARE_DISK_BLOCK_SIZE; i++){
        if(mainMap.bytes[i] == 0){
            index = i;
            mainMap.bytes[i] = 1;
            break;
        }
    }

    directory_size++;
    DirEntry dir;
    dir.file_is_open = 0;
    strcpy(dir.name, name);
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
void close_file(File file){

}


// read at most 'numbytes' of data from 'file' into 'buf', starting at the 
// current file position.  Returns the number of bytes read. If end of file is reached,
// then a return value less than 'numbytes' signals this condition. Always sets
// 'fserror' global.
unsigned long read_file(File file, void *buf, unsigned long numbytes){

}

// write 'numbytes' of data from 'buf' into 'file' at the current file position. 
// Returns the number of bytes written. On an out of space error, the return value may be
// less than 'numbytes'.  Always sets 'fserror' global.
unsigned long write_file(File file, void *buf, unsigned long numbytes){

}

// sets current position in file to 'bytepos', always relative to the
// beginning of file.  Seeks past the current end of file should
// extend the file. Returns 1 on success and 0 on failure.  Always
// sets 'fserror' global.
int seek_file(File file, unsigned long bytepos){

}

// returns the current length of the file in bytes. Always sets 'fserror' global.
unsigned long file_length(File file){

}


// deletes the file named 'name', if it exists. Returns 1 on success, 0 on failure. 
// Always sets 'fserror' global.   
int delete_file(char *name){

}

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name){

}



// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void){
    
}
//main is in here for now just to make testing easy, we're gonna move to formatfs.c later

int main(){
    init_software_disk();

    //FOR NOW THIS IS USING BYTES, NOT BITS!! CHANGE IT!
    mainMap.bytes[0] = 1;
    write_sd_block(&mainMap, BITMAP_DISK_BLOCK); //NOT SURE HOW TO USE THIS! BUT IT'S GOTTA GO IN BLOCK 0!
    printf("This is the disk: %d\n", software_disk_size());
    for(int i = 0; i <= 20; i++){
        printf("%d", mainMap.bytes[i]);
    }
    printf("\n");
    File new = create_file("one");
    create_file("two");
    create_file("three");
    create_file("four");
    for(int i = 0; i <= 20; i++){
        printf("%d", mainMap.bytes[i]);
    }

    DirEntry testDir;
    int ret = read_sd_block(&testDir, 2);
    printf("\nThis is what I found: %s", testDir.name);
    return 0;
}

