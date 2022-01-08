#ifndef STATE_H
#define STATE_H

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define MAX_SUPPL_REFS (BLOCK_SIZE / sizeof(int))

/*
 * Directory entry
 */
typedef struct {
    char d_name[MAX_FILE_NAME];
    // @foo index of corresponding inode in inode table
    int d_inumber;
} dir_entry_t;

typedef enum { T_FILE, T_DIRECTORY } inode_type;

/*
 * Ex.1 Set of indexes pointing to data blocks. Allocated if need be when tfs_write is called.
 */
typedef struct {
    int indexes[MAX_SUPPL_REFS];
} i_block;

/*
 * I-node
 */
typedef struct {
    inode_type i_node_type;
    size_t i_size;
    int i_data_blocks[MAX_DIRECT_REFS];
    int i_data_block;
    i_block* i_block;
    /* in a real FS, more fields would exist here */
} inode_t;


typedef enum { FREE = 0, TAKEN = 1 } allocation_state_t;

/*
 * Open file entry (in open file table)
 */
typedef struct {
    int of_inumber;
    // @foo cursor position
    size_t of_offset;
} open_file_entry_t;

#define MAX_DIR_ENTRIES (BLOCK_SIZE / sizeof(dir_entry_t))

void state_init();
void state_destroy();

int inode_create(inode_type n_type);
int inode_delete(int inumber);
inode_t *inode_get(int inumber);

int clear_dir_entry(int inumber, int sub_inumber);
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name);
int find_in_dir(int inumber, char const *sub_name);

int data_block_alloc();
int data_block_free(inode_t* inode);
//TODO
i_block* i_block_alloc();
int i_block_free(i_block* iblock);

void *data_block_get(int block_number);

int add_to_open_file_table(int inumber, size_t offset);
int remove_from_open_file_table(int fhandle);
open_file_entry_t *get_open_file_entry(int fhandle);

#endif // STATE_H
