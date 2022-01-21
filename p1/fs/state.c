#include "state.h"
#include "config.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
pthread_rwlock_t inodelock;
static inode_t inode_table[INODE_TABLE_SIZE];
static char freeinode_ts[INODE_TABLE_SIZE];

/* Data blocks */
pthread_rwlock_t datalock;
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static char free_blocks[DATA_BLOCKS];

/* Volatile FS state */

pthread_rwlock_t oftlock;
static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static char free_open_file_entries[MAX_OPEN_FILES];

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
    }
}

void state_destroy() {
    int i;
    for (i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_t* inode = inode_get(i);
        if (inode != NULL) {
            inode_delete(i);
        }
    }
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {
    pthread_rwlock_wrlock(&inodelock);
    pthread_rwlock_wrlock(&datalock);
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int) sizeof(allocation_state_t) % BLOCK_SIZE) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }
        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
            /* Found a free entry, so takes it for the new i-node*/
            freeinode_ts[inumber] = TAKEN;
            insert_delay(); // simulate storage access delay (to i-node)
            inode_table[inumber].i_node_type = n_type;

            if (n_type == T_DIRECTORY) {
                /* Initializes directory (filling its block with empty
                 * entries, labeled with inumber==-1) */
                int b = data_block_alloc();
                if (b == -1) {
                    freeinode_ts[inumber] = FREE;
                    return -1;
                }

                inode_table[inumber].i_size = BLOCK_SIZE;
                inode_table[inumber].i_data_block = b;

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);
                if (dir_entry == NULL) {
                    freeinode_ts[inumber] = FREE;
                    return -1;
                }

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                /* In case of a new file, simply sets its size to 0 */
                inode_table[inumber].i_size = 0;
                inode_table[inumber].i_data_block = -1;
                for (int i = 0; i < MAX_DIRECT_REFS; i++) {
                    inode_table[inumber].i_data_blocks[i] = data_block_alloc();
                    if (inode_table[inumber].i_data_blocks[i] == -1) {
                        freeinode_ts[inumber] = FREE;
                        return -1;
                    }
                }
                inode_table[inumber].i_block = NULL;
            }
            pthread_rwlock_unlock(&inodelock);
            pthread_rwlock_unlock(&datalock);
            return inumber;
        }
    }
    return -1;
}

/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();
    pthread_rwlock_wrlock(&inodelock);
    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE) {
        return -1;
    }

    freeinode_ts[inumber] = FREE;

    if (inode_table[inumber].i_size > 0) {
        if (data_blocks_free(&inode_table[inumber]) == -1) {
            return -1;
        }
    }
    pthread_rwlock_unlock(&inodelock);
    return 0;
}

void* i_block_get(int index, i_block* iblock) {
    return &fs_data[iblock->indexes[index] * BLOCK_SIZE];
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    return &inode_table[inumber];
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to i-node with inumber
    pthread_rwlock_rdlock(&inodelock);
    if (inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }
    pthread_rwlock_unlock(&inodelock);

    if (strlen(sub_name) == 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&datalock);
    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block);
    if (dir_entry == NULL) {
        return -1;
    }

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
        if (dir_entry[i].d_inumber == -1) {
            dir_entry[i].d_inumber = sub_inumber;
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;
            pthread_rwlock_unlock(&datalock);
            return 0;
        }
    }
    pthread_rwlock_unlock(&datalock);
    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber
    pthread_rwlock_rdlock(&inodelock);
    if (!valid_inumber(inumber) ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }
    pthread_rwlock_unlock(&inodelock);

    pthread_rwlock_wrlock(&datalock);
    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block);
    if (dir_entry == NULL) {
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++)
        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {
            pthread_rwlock_unlock(&datalock);
            return dir_entry[i].d_inumber;
        }
    pthread_rwlock_unlock(&datalock);
    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {
    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

        if (free_blocks[i] == FREE) {
            free_blocks[i] = TAKEN;
            return i;
        }
    }
    return -1;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks
    pthread_rwlock_wrlock(&datalock);
    free_blocks[block_number] = FREE;
    pthread_rwlock_unlock(&datalock);
    return 0;
}

/* Frees one or more data blocks
 * Input
 * 	- the inode
 * Returns: 0 if success, -1 otherwise
 */
int data_blocks_free(inode_t* inode) {
    if (inode->i_node_type == T_DIRECTORY) {
        if (!valid_block_number(inode->i_data_block)) {
            return -1;
        }

        insert_delay(); // simulate storage access delay to free_blocks
        pthread_rwlock_wrlock(&datalock);
        free_blocks[inode->i_data_block] = FREE;
        pthread_rwlock_unlock(&datalock);
        return 0;
    }
    else {
        int taken_blocks = (int) (inode->i_size / BLOCK_SIZE) + 1;
        if (taken_blocks > MAX_DIRECT_REFS)
            taken_blocks = MAX_DIRECT_REFS;
        pthread_rwlock_wrlock(&datalock);
        for (int i = 0; i < taken_blocks; ++i) {
            if (!valid_block_number(inode->i_data_blocks[i])) {
            return -1;
            }

            insert_delay(); // simulate storage access delay to free_blocks
            free_blocks[inode->i_data_blocks[i]] = FREE;
        }
        i_block_free(inode->i_block);
        pthread_rwlock_unlock(&datalock);
        return 0;
    }
}

/* Allocates an i_block. Returns a pointer to the i_block. */

i_block* i_block_alloc() {
    i_block* b =  malloc(sizeof(i_block));
    pthread_rwlock_wrlock(&datalock);
    for (int i = 0; i < MAX_SUPPL_REFS; i++) {
        b->indexes[i] = data_block_alloc();
    }
    pthread_rwlock_unlock(&datalock);
    return b;
}

/* Frees an i_block.
 * Input:
 * - Pointer to i_block
 * Returns: 0 if successful, -1 otherwise
 */
int i_block_free(i_block *iblock) {
    if (iblock == NULL)
        return 0;
    for (int i = 0; i < MAX_SUPPL_REFS; i++) {
        if (data_block_free(iblock->indexes[i]) == -1)
            return -1;
    }
    free(iblock);
    return 0;
}

/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block
    char* res = &fs_data[block_number * BLOCK_SIZE];
    return res;
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {
    pthread_rwlock_wrlock(&oftlock);
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == FREE) {
            free_open_file_entries[i] = TAKEN;
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;
            return i;
        }
    }
    pthread_rwlock_unlock(&oftlock);
    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 if successful, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    pthread_rwlock_wrlock(&oftlock);
    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
        return -1;
    }
    free_open_file_entries[fhandle] = FREE;
    pthread_rwlock_unlock(&oftlock);
    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }
    pthread_rwlock_rdlock(&oftlock);
    open_file_entry_t* res = &open_file_table[fhandle];
    pthread_rwlock_unlock(&oftlock);
    return res;
}
