#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "disk.h"
#include <sys/wait.h>

//1 directory only
//maximum 64 files
//maximum 15 char file names
//WE HAVE 8K BLOCKS OF SIZE 4K

#define MAX_FILE 64
#define MAX_FILDES 32
#define MAX_F_NAME 15
#define MAX_FILE_SIZE (4096 * 4096)

#define RESERVED -2
#define FREE 0
#define eof -1

#define UNUSED -1
#define USED 1

#define NOT_SET -3

int *FAT;
int made = -1;
int mounted = -1;

struct super_block { //ONLY ONE SUPER BLOCK
    int fat_idx; // First block of the FAT
    int fat_len; // Length of FAT in blocks
    int dir_idx; // First block of directory
    int dir_len; // Length of directory in blocks
    int data_idx; // First block of file-data
};
struct super_block *fs;

struct dir_entry { //UP TO 64 DIRECTORY ENTRIES
    int used; // Is this file-”slot” in use
    char name [MAX_F_NAME + 1];
    int size; // file size
    int head; // first data block of file
    int ref_cnt;
    //how many open file descriptors are there?
    //ref_cnt > 0 -> cannot delete file
};
struct dir_entry *DIR;

struct file_descriptor { //UP TO 32 FILE DESCRIPTORS
    int used; // fd in use
    int file; // the first block of the file
    //(f) to which fd refers too
    int offset; // position of fd within f
};
struct file_descriptor fildes[MAX_FILDES];


int make_fs(char *disk_name) {
    printf("Building file system\n");
    int disk = make_disk(disk_name);
    if (disk == -1) {
        perror("MAKE_DISK FAILED");
        return -1;
    }
    int disk_open = open_disk(disk_name);
    if (disk_open == -1) {
        perror("OPEN_DISK FAILED");
        return -1;
    }

    made = 1;

    fs = calloc(1, BLOCK_SIZE); //SUPER BLOCK
    fs->fat_idx = 1; // First block of the FAT
    fs->fat_len = 8; // Length of FAT in blocks
    fs->dir_idx = fs->fat_idx + fs->fat_len; // First block of directory
    fs->dir_len = 1; // Length of directory in blocks
    fs->data_idx = 10; // First block of file-data
    int fs_write = block_write(0, (char *) fs);
    if (fs_write == -1) {
        perror("WRITE SUPERBLOCK FAILED");
        return -1;
    }

    FAT = calloc(DISK_BLOCKS, sizeof(int));            //FAT
    for (int i = 0; i < fs->data_idx; i++)
        FAT[i] = RESERVED;
    for (int i = fs->data_idx; i < DISK_BLOCKS; i++)
        FAT[i] = FREE;
    for (int i = 0; i < fs->fat_len; i++) {
        if (block_write(fs->fat_idx + i, (char*) (FAT) + i * BLOCK_SIZE) == -1) {
            perror("WRITE FAT FAILED");
            return -1;
        }
    }

    DIR = calloc(MAX_FILE, sizeof(struct dir_entry));
    for (int i = 0; i < MAX_FILE; i++) {
        DIR[i].used = UNUSED;
        DIR[i].size = NOT_SET;
        DIR[i].head = NOT_SET;
        DIR[i].ref_cnt = 0;
        DIR[i].name[0] = ' ';
    }
    char *buf = malloc(BLOCK_SIZE);
    memcpy((void* ) buf, (void *) DIR, sizeof(struct dir_entry));
    int DIR_write = block_write(fs->dir_idx, buf);
    if (DIR_write == -1) {
        perror("DIRECTORY WRITE TO DISK FAILED");
        return -1;
    }
    free(buf);

    if (close_disk() < 0) {
        perror("COULD NOT CLOSE DISK");
        return -1;
    }
//    free(fs);
//    free(DIR);
//    free(FAT);
    return 0;
}

int mount_fs(char *disk_name) {
    printf("MOUNTING FILE SYSTEM\n");

    int disk_open = open_disk(disk_name);
    if (disk_open < 0) {
        perror("OPEN_DISK FAILED");
        return -1;
    }

    mounted = 1;
    //made = 1; //TEMPORARY!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    //INITIALIZE SUPER BLOCK
    if (made < 0) {
        fs = calloc(1, BLOCK_SIZE);
//        fs->fat_idx = 1; // First block of the FAT
//        fs->fat_len = 8; // Length of FAT in blocks
//        fs->dir_idx = fs->fat_idx + fs->fat_len; // First block of directory
//        fs->dir_len = sizeof(DIR) / BLOCK_SIZE ; // Length of directory in blocks
//        fs->data_idx = fs->dir_idx + fs->dir_len; // First block of file-data
          int fs_read = block_read(0, (char*) fs);
          if (fs_read < 0) {
              perror("READ FS FAILED");
              return -1;
          }
//        fs->fat_idx = 1; // First block of the FAT
//        fs->fat_len = 8; // Length of FAT in blocks
//        fs->dir_idx = fs->fat_idx + fs->fat_len; // First block of directory
//        fs->dir_len = 1; // Length of directory in blocks
//        fs->data_idx = 10; // First block of file-data
    }

    //GET FAT FROM SAVED DISK
    if (made < 0) {
        char *FAT_buf = malloc(BLOCK_SIZE);
        FAT = calloc(DISK_BLOCKS, sizeof(int));
        int FAT_read;
        for (int i = 0; i < fs->fat_len; i++) {
            FAT_read = block_read(fs->fat_idx + i, FAT_buf);
            memcpy((void *) FAT + i * BLOCK_SIZE, (void *) FAT_buf, BLOCK_SIZE);
            if (FAT_read < 0) {
                perror("CAN'T RETRIEVE THE FAT");
                return -1;
            }
        }
        free(FAT_buf);
    }
    //INITIALIZE DIRECTORY
    if (made < 0) {
        char *DIR_buf = malloc(BLOCK_SIZE);
        int DIR_read = block_read(fs->dir_idx, DIR_buf);
        if (DIR_read < 0) {
            perror("COULD NOT RETRIEVE THE DIRECTORY");
            return -1;
        }
        DIR = calloc(fs->dir_len, BLOCK_SIZE);
        memcpy((void *) DIR, (void *) DIR_buf, sizeof(struct dir_entry));
        free(DIR_buf);
    }




    //INITIALIZE FILE DESCRIPTORS
    for (int i = 0; i < MAX_FILDES; i++) {
        fildes[i].used = UNUSED;
        fildes[i].file = UNUSED;
        fildes[i].offset = 0;
    }

//    if (made == -1) {
//        free(fs);
//        free(FAT);
//        free(DIR);
//    }

    return 0;
}

int umount_fs(char *disk_name) {
    printf("UNMOUNTING FILE SYSTEM\n");
    if (mounted < 0) {
        perror("FILE SYSTEM WAS NOT MOUNTED");
        return -1;
    }

    //SAVE FILE SYSTEM TO VIRTUAL DISK
    int write_to_disk = block_write(0, (char *) fs); //SAVE THE SUPER BLOCK
    if (write_to_disk < 0) {
        perror("SUPER BLOCK WRITE TO DISK FAILED");
        return -1;
    }

    char *FAT_buf = calloc(1, BLOCK_SIZE);
    for (int i = 0; i < fs->fat_len; i++) { //SAVE ALL OF THE FAT BLOCKS
        memcpy((void *) FAT_buf, ((void *) FAT) + i * BLOCK_SIZE, BLOCK_SIZE);
        write_to_disk = block_write(fs->fat_idx + i, FAT_buf);
        if (write_to_disk < 0) {
            perror("FAT WRITE TO DISK FAILED");
            return -1;
        }
    }
    free(FAT_buf);

    char *DIR_buf = calloc(1, BLOCK_SIZE);
    memcpy((void *) DIR_buf, (void *) DIR, MAX_FILE * sizeof(struct dir_entry));
    write_to_disk = block_write(fs->dir_idx, DIR_buf);
    if (write_to_disk < 0) {
        perror("DIRECTORY WRITE TO DISK FAILED");
        return -1;
    }
    free(DIR_buf);
    int close = close_disk();
    if (close < 0) {
        perror("CLOSE_DISK FAILED");
        return -1;
    }
    free(fs);
    free(FAT);
    free(DIR);
    mounted = -1;
    return 0;
}


int fs_open(char *name) { //file specified by name is opened for reading and writing, and the
    // file descriptor corresponding to this file is returned to the calling function
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }

    int file_num, file_name_found = -1;
    for (file_num = 0; file_num < MAX_FILE; file_num++) {
        if (strcmp(DIR[file_num].name, name) == 0) {
            file_name_found = 1;
            break;
        }
    }
    if (file_name_found == -1) {
        perror("NO FILE WITH MATCHING NAME FOUND");
        return -1;
    }

    int fildes_num, open_fildes_found = -1;
    for (fildes_num = 0; fildes_num < MAX_FILDES; fildes_num++) {
        if (fildes[fildes_num].used == UNUSED) {
            fildes[fildes_num].used = USED;
            fildes[fildes_num].offset = 0;
            fildes[fildes_num].file = DIR[file_num].head;
            DIR[file_num].ref_cnt++;
            open_fildes_found = 1;
            break;
        }
    }
    if (open_fildes_found == -1) {
        perror("NO OPEN FILE DESCRIPTORS");
        return -1;
    }
    return fildes_num;
}

int fs_close(int fildes_num) {  //file descriptor fildes is closed.
    // A closed file descriptor can no longer be used to access the corresponding file.
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    if (fildes[fildes_num].used == UNUSED) {
        perror("FILDES DOES NOT CORRESPOND TO AN OPEN FILE");
        return -1;
    }
    int fildes_found = -1;
    for (int i = 0; i < MAX_FILE; i++) {
        if (DIR[i].head == fildes[fildes_num].file) {
            fildes[fildes_num].used = UNUSED;
            fildes[fildes_num].offset = NOT_SET;
            fildes[fildes_num].file = NOT_SET;
            DIR[i].ref_cnt--;
            fildes_found = 1;
            break;
        }
    }
    if (fildes_found == -1) {
        perror("FILE DESCRIPTOR NOT FOUND");
        return -1;
    }
    return 0;
}

int fs_create(char *name) { //function creates a new file with name name in the root directory of your file system
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    printf("CREATING FILE\n");

    for (int i = 0; i  < MAX_FILE; i++) {
        if (strcmp(name, DIR[i].name) == 0) {
            perror("A FILE WITH THIS NAME ALREADY EXISTS");
            return -1;
        }
    }

    int file_slot, found_file_slot = -1;
    for (file_slot = 0; file_slot < MAX_FILE; file_slot++) {
        if (DIR[file_slot].used == UNUSED) {
            found_file_slot = 1;
            break;
        }
    }
    if (found_file_slot == -1) {
        perror("NO EMPTY FILE SLOTS");
        return -1;
    }

    int open_FAT_block, found_open_FAT_block = -1;
    for (open_FAT_block = fs->data_idx; open_FAT_block < DISK_BLOCKS; open_FAT_block++) {
        if (FAT[open_FAT_block] == FREE) {
            found_open_FAT_block = 1;
            FAT[open_FAT_block] = eof;
            break;
        }
    }
    if (found_open_FAT_block == -1) {
        perror("NO EMPTY FAT BLOCKS");
        return -1;
    }

    stpcpy(DIR[file_slot].name, name);
    DIR[file_slot].used = USED;
    DIR[file_slot].ref_cnt = 0;
    DIR[file_slot].head = open_FAT_block;
    DIR[file_slot].size = 0;

    return 0;
}

int fs_delete(char *name) { //deletes the file with name name from the root directory of your file system
    // frees all data blocks and meta-information that correspond to that file
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }

    int file_name, found_file_name = -1;
    for (file_name = 0; file_name < MAX_FILE; file_name++) {
        if (strcmp(DIR[file_name].name, name) == 0) {
            found_file_name = 1;
            if (DIR[file_name].ref_cnt > 0) {
                perror("FILE STILL IN USE");
                return -1;
            }
            break;
        }
    }
    if (found_file_name == -1) {
        perror("FILE NAME NOT FOUND");
        return -1;
    }

    int FAT_iterator = DIR[file_name].head;
    while (FAT[FAT_iterator] != EOF) {
        int temp = FAT[FAT_iterator];
        FAT[FAT_iterator] = FREE;
        FAT_iterator = temp;
    }

    stpcpy(DIR[file_name].name, " ");
    DIR[file_name].head = UNUSED;
    DIR[file_name].ref_cnt = 0;
    DIR[file_name].size = 0;
    DIR[file_name].used = UNUSED;

    return 0;
}

int fs_read(int fildes_num, void *buf, size_t nbyte) { //attempts to read nbyte bytes of data from the file referenced by the descriptor fildes into the buffer pointed to by buf.
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    if (nbyte == 0) {
        printf("Nothing to read!\n");
        return 0;
    }

    if (fildes[fildes_num].used == UNUSED) {
        perror("THIS FILE DESCRIPTOR IS UNUSED");
        return -1;
    }
    int file_found = -1, file;
    for (file = 0; file < MAX_FILE; file++) {
        if (DIR[file].head == fildes[fildes_num].file) {
            file_found = 1;
            break;
        }
    }
    if (file_found == -1) {
        perror("FILE NOT FOUND");
        return -1;
    }

    int offset = fildes[fildes_num].offset;

    if (DIR[file].size - offset < nbyte)
        nbyte = DIR[file].size - offset;

    //FIND THE BEGINNING OF WHERE WE WANT TO READ FROM
    int file_tracer = fildes[fildes_num].file;
    while (offset > BLOCK_SIZE) {
        file_tracer = FAT[file_tracer];
        offset -= BLOCK_SIZE;
    }
    //file_tracer += offset;

    int num_bytes_already_read = 0;
    char current_block[BLOCK_SIZE];
    char * read_array = (char *) calloc(nbyte , sizeof(char));
    if (block_read(file_tracer, current_block) == -1) {
        perror("READ FAILED");
        return -1;
    }
    //READ TO END OF CURRENT BLOCK
    int to_end_block = BLOCK_SIZE - offset;
    if (nbyte < to_end_block) {//WE ONLY NEED TO READ NBYTE

        for (int i = 0 + offset, j = 0; i < offset + nbyte; i++, j++) {
            read_array[j] = current_block[i];
            fildes[fildes_num].offset++;
        }
        memcpy(buf, read_array, nbyte);
        //fildes[fildes_num].offset += nbyte;
        free(read_array);
        return nbyte;
    }
    else {
        for (int i = 0 + offset; i < BLOCK_SIZE; i++) { //WE NEED TO READ TO THE END OF THE BLOCK
            read_array[i] = current_block[i];
        }
        num_bytes_already_read += (BLOCK_SIZE - offset);
        fildes[fildes_num].offset += (BLOCK_SIZE - offset);
    }
    //READ THE FULL BLOCKS
    int num_full_blocks = (nbyte - to_end_block) / 4096;
    for (int i = 0; i < num_full_blocks; i++) {
        file_tracer = FAT[file_tracer];

        if (block_read(file_tracer, current_block) == -1) {
            perror("READ FAILED");
            free(read_array);
            return -1;
        }
        for (int j = 0; j < BLOCK_SIZE; j++) {
            read_array[j + num_bytes_already_read] = current_block[j];
        }
        num_bytes_already_read += BLOCK_SIZE;
        fildes[fildes_num].offset += BLOCK_SIZE;
    }

    //READ LEFTOVERS
    int leftovers = nbyte - num_bytes_already_read;
    file_tracer = FAT[file_tracer];
    if (block_read(file_tracer, current_block) == -1) {
        perror("READ FAILED");
        free(read_array);
        return -1;
    }
    for (int j = 0; j < leftovers; j++) {
        read_array[j + num_bytes_already_read] = current_block[j];
    }
    num_bytes_already_read += leftovers;
    fildes[fildes_num].offset += leftovers;

    memcpy(buf, read_array, nbyte);
    free(read_array);
    return num_bytes_already_read;
}

int fs_write(int fildes_num, void *buf, size_t nbyte) { // attempts to write nbyte bytes of data to the file referenced
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }                                                    // by the descriptor fildes from the buffer pointed to by buf
    if (nbyte == 0) {
        printf("Nothing to write!\n");
        return 0;
    }
    long int nbyte_copy = nbyte;
    if (fildes[fildes_num].used == UNUSED) {
        perror("THIS FILE DESCRIPTOR IS UNUSED");
        return -1;
    }
    if (nbyte_copy + fildes[fildes_num].offset > MAX_FILE_SIZE) {
        printf("WRITE WOULD HAVE MADE FILE TOO LARGE, SHORTENING\n");
        nbyte_copy = MAX_FILE_SIZE - fildes[fildes_num].offset;
    }
    if (nbyte_copy <= 0) {
        perror("NOWHERE TO WRITE");
        return 0;
    }

    //FIND THIS FILE IN THE DIRECTORY
    int DIR_num;
    for (DIR_num = 0; DIR_num < MAX_FILE; DIR_num++) {
        if (DIR[DIR_num].head == fildes[fildes_num].file)
            break;
    }

    int offset = fildes[fildes_num].offset;
    int original_offset = fildes[fildes_num].offset;
    //FIND THE BEGINNING OF WHERE WE WANT TO WRITE TO
    int file_tracer = fildes[fildes_num].file;
    int count = 0;
    while (offset >= BLOCK_SIZE) {
        file_tracer = FAT[file_tracer];
        offset -= BLOCK_SIZE;
        count++;
        //printf("%d\n", file_tracer);
    }

//    int tracer2 = file_tracer;
//    int num_FAT_blocks = 0;
//    while (FAT[tracer2] != eof && FAT[tracer2] != RESERVED) {
//        num_FAT_blocks++;
//        tracer2 = FAT[tracer2];
//    }


    int num_full_blocks_to_be_written = (nbyte_copy - (BLOCK_SIZE - offset)) / BLOCK_SIZE; //CALCULATING NUMBER OF FULL BLOCKS TO WRITE
    //int blocks_already_written = (DIR[DIR_num].size - offset) / BLOCK_SIZE;
    //int USE_TRACER_ONE = -1;
    //int USE_TRACER_TWO = -1;
    //IF FILE IS EMPTY, find next empty FAT block, make that EOF, fill in current block
//    if (DIR[DIR_num].size == 0) {
//        FAT[file_tracer] = FREE;
//        //USE_TRACER_ONE = 1;
//        int blocks_needed = (nbyte_copy + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
//        int prev_block = file_tracer;
//        for (int i = 0; i < blocks_needed; i++) { //Look for as many blocks as we need
//            int open_FAT_block;
//            for (open_FAT_block = fs->data_idx; open_FAT_block < DISK_BLOCKS; open_FAT_block++) {//find empty block
//                if (FAT[open_FAT_block] == FREE) {
//                    FAT[open_FAT_block] = eof;
//                    FAT[prev_block] = open_FAT_block;
//                    prev_block = open_FAT_block;
//                    break;
//                }
//            }
//        }
//    if (num_full_blocks_to_be_written + 1 > DIR[DIR_num].size - original_offset) { //FAT IS NOT EMPTY BUT WE STILL NEED MORE BLOCKS
//        FAT[file_tracer] = FREE;
//        //USE_TRACER_TWO = 1;
//        int blocks_needed = num_full_blocks_to_be_written - (DIR[DIR_num].size - original_offset + BLOCK_SIZE - 1) / BLOCK_SIZE + 1;
//
//        int prev_block = file_tracer;
//        for (int i = 0; i < blocks_needed; i++) { //Look for as many blocks as we need
//            int open_FAT_block;
//            for (open_FAT_block = prev_block; open_FAT_block < DISK_BLOCKS; open_FAT_block++) {//find empty block
//                if (FAT[open_FAT_block] == FREE) {
//                    FAT[open_FAT_block] = eof;
//                    FAT[prev_block] = open_FAT_block;
//                    prev_block = open_FAT_block;
//                    break;
//                }
//                //printf("hi");
//            }
//        }
//        //file_tracer = tracer2;
//    }
    int bytes_that_need_space = nbyte_copy - (DIR[DIR_num].size - original_offset);
    int blocks_that_need_space = (bytes_that_need_space + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int first_block = file_tracer;
    int prev_block = first_block;
    for (int i = 0; i < blocks_that_need_space; i++) {

            for (int j = first_block; j < DISK_BLOCKS; j++) {//find empty block
                if (FAT[j] == FREE) {
                    FAT[prev_block] = j;
                    prev_block = j;
                    FAT[j] = eof;
                    break;
                }
            }
    }


    //char temp_buf[nbyte]; //WE'LL USE THIS BUFFER TO WRITE INTO THE BLOCKS
    char * temp_buf = (char *) malloc(nbyte_copy * sizeof(char));
    int bytes_written = 0; //KEEP TRACK OF HOW MUCH WE'VE ALREADY WRITTEN
    memcpy(temp_buf, buf, nbyte_copy);

    char current_block[BLOCK_SIZE] = " ";

    if (block_read(file_tracer, current_block) == -1) { //READ OUT THE BLOCKS SO WE CAN EDIT THEM THEN WRITE THEM BACK
        perror("READ FAILED");
        return -1;
    }

    int unused_bytes = 0;
    int buf_index = 0;
    for (int i = 0;  i < BLOCK_SIZE; i++) {

        if (i >= offset) {
            current_block[i] = temp_buf[buf_index];
            buf_index++;
            nbyte_copy--;
            fildes[fildes_num].offset++;
            if (fildes[fildes_num].offset >= DIR[DIR_num].size) {
                DIR[DIR_num].size = fildes[fildes_num].offset;
            }
        } else {
            unused_bytes++;
        }
        if (nbyte_copy == 0) {

            block_write(file_tracer, current_block);
            //fildes[fildes_num].offset += i + 1 - unused_bytes;
            printf("Offset is now %d\n", fildes[fildes_num].offset);
            free(temp_buf);
            return i + 1 - unused_bytes; //SHOULD RETURN HERE IF WE HAVE WRITTEN WITHIN A SINGLE BLOCK
        }
    }

    block_write(file_tracer, current_block);
    bytes_written += (BLOCK_SIZE - offset);
    //fildes[fildes_num].offset += (BLOCK_SIZE - offset);
    //DIR[DIR_num].size += (BLOCK_SIZE - offset);

    for (int i = 0; i < num_full_blocks_to_be_written; i++) { //WRITE THE FULL BLOCKS
        file_tracer = FAT[file_tracer];
        if (file_tracer == -1) {
            printf("WE HAVE RUN OUT OF SPACE!\n");
            free(temp_buf);
            return bytes_written;
        }
        if (block_read(file_tracer, current_block) == -1) {
            perror("READ FAILED");
            free(temp_buf);
            return -1;
        }

        for (int j = 0; j < BLOCK_SIZE; j++)
            current_block[j] = temp_buf[j + bytes_written];

        block_write(file_tracer, current_block);
        bytes_written += BLOCK_SIZE;
        fildes[fildes_num].offset += BLOCK_SIZE;
        if (fildes[fildes_num].offset > DIR[DIR_num].size)
            DIR[DIR_num].size += fildes[fildes_num].offset -DIR[DIR_num].size;
        nbyte_copy -= BLOCK_SIZE;
    }
    if (nbyte_copy <= 0) {
        printf("Offset after writing is %d, \n", fildes[fildes_num].offset);
        printf("WROTE UP UNTIL BLOCK %d\n", file_tracer);
        return bytes_written;
    }

    //WRITE LEFTOVERS
    file_tracer = FAT[file_tracer];
    if (file_tracer == -1) {
        printf("WE HAVE RUN OUT OF SPACE! 2");
        free(temp_buf);
        return bytes_written;
    }
    if (block_read(file_tracer, current_block) == -1) {
        perror("READ FAILED");
        free(temp_buf);
        return -1;
    }
    for (int i = 0; i < BLOCK_SIZE; i++) {
        current_block[i] = temp_buf[i + bytes_written];
        nbyte_copy--;
        if (nbyte_copy == 0) {
            block_write(file_tracer, current_block);
            fildes[fildes_num].offset += (i + 1);
            printf("Offset is now %d\n", fildes[fildes_num].offset);
            if (fildes[fildes_num].offset > DIR[DIR_num].size)
                DIR[DIR_num].size += fildes[fildes_num].offset -DIR[DIR_num].size;
            free(temp_buf);
            return bytes_written + i + 1;
        } else if (nbyte_copy == -1) { //MAYBEEEEEEEEE
            block_write(file_tracer, current_block);
            fildes[fildes_num].offset += i;
            printf("Offset is now %d\n", fildes[fildes_num].offset);
            if (fildes[fildes_num].offset > DIR[DIR_num].size)
                DIR[DIR_num].size += fildes[fildes_num].offset -DIR[DIR_num].size;
            free(temp_buf);
            return bytes_written + i;
        }
    }
    //block_write(file_tracer, current_block);
    //bytes_written += (nbyte - bytes_written);
    //fildes[fildes_num].offset += (nbyte - bytes_written);
    //DIR[DIR_num].size += (nbyte - bytes_written);
    free(temp_buf);
    return DIR[DIR_num].size - original_offset;
}

int fs_get_filesize(int fildes_num) { //This function returns the current size of the file referenced by the file descriptor fildes
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    if (fildes[fildes_num].used == UNUSED) {
        perror("THIS FILE DESCRIPTOR IS UNUSED");
        return -1;
    }
    int file_found = -1, file;
    for (file = 0; file < MAX_FILE; file++) {
        if (DIR[file].head == fildes[fildes_num].file) {
            file_found = 1;
            break;
        }
    }
    if (file_found == -1) {
        perror("FILE NOT FOUND");
        return -1;
    }
    else
        return DIR[file].size;
}

int fs_listfiles(char ***files) { // creates and populates an array of all filenames currently known to the file system.
    //should add a NULL pointer after the last element in the array
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    printf("CREATING LIST OF ALL FILES\n");
    char ** all_files = {NULL};
    all_files = (char **) calloc(65, sizeof(char *));
    int file_num = 0;
    for (int i = 0; i < MAX_FILE; i++) {
        if (DIR[i].used == USED) {
            all_files[i] = (char *) calloc(16, sizeof(char));
            all_files[file_num] = DIR[i].name;
            file_num++;
        }
    }
    //char * short_all_files[file_num + 1];
    //for (int i = 0; i < file_num; i++)
    //    short_all_files[i] = all_files[i];
    //short_all_files[file_num] = NULL;
    //files = (char ***) calloc(file_num + 1, sizeof(char *));
    printf("allocated\n");
    //for (int i = 0; i < file_num; i++)
    //    strcpy(*files[i], all_files[i]);
    *files = all_files;
    printf("returning\n");
    return 0;
}

int fs_lseek(int fildes_num, off_t offset) { //sets the file pointer (the offset used for read and write operations) associated with the file descriptor fildes to the argument offset.
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    printf("SETTING OFFSET TO %ld\n", offset);
    if (fildes[fildes_num].used == UNUSED) {
        perror("THIS FILE DESCRIPTOR IS UNUSED");
        return -1;
    }
    if (fildes_num < 0 || fildes_num >= MAX_FILDES) {
        perror("OUT OF BOUNDS");
        return -1;
    }
    if (offset < 0 || offset > fs_get_filesize(fildes_num)) {
        perror("INVALID OFFSET");
        return -1;
    }

    fildes[fildes_num].offset = offset;

    return 0;
}

int fs_truncate(int fildes_num, off_t length) { // causes the file referenced by fildes to be truncated to length bytes in size
    if (mounted == -1) {
        perror("DISK NOT MOUNTED");
        return -1;
    }
    if (fildes[fildes_num].used == UNUSED) {
        perror("THIS FILE DESCRIPTOR IS UNUSED");
        return -1;
    }
    int file_found = -1, file;
    for (file = 0; file < MAX_FILE; file++) {
        if (DIR[file].head == fildes[fildes_num].file) {
            file_found = 1;
            break;
        }
    }
    if (file_found == -1) {
        perror("FILE NOT FOUND");
        return -1;
    }
    if (DIR[file].size < length) {
        perror("NO NEED TO TRUNCATE, ALREADY SHORT ENOUGH");
        return -1;
    }
    int num_blocks = (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int DIR_blocks = (DIR[file].size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    if (DIR_blocks != num_blocks) {
        int FAT_iterator = DIR[file].head, count = 0;// changed = -1;
        while (FAT_iterator != EOF) {
            int temp = FAT[FAT_iterator];
            if (count == num_blocks) {
                FAT_iterator = EOF;
                //changed = 1;
            } else if (count > num_blocks)
                FAT_iterator = FREE;
            FAT_iterator = temp;
            count++;
        }
    }
//    if (changed == 1)
//        FAT_iterator = FREE;

    if (fildes[file].offset > length)
        fildes[file].offset = length;
    DIR[file].size = length;

    return 0;
}
