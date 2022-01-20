#include "operations.h"
#include "config.h"
#include "state.h"
#include "math.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

pthread_rwlock_t rwlock;

int tfs_init() {
    state_init();

    /* create root inode.*/
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;
    int res = find_in_dir(ROOT_DIR_INUM, name);
    return res;
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    
    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }
    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Truncate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (data_blocks_free(inode) == -1) {
                    return -1;
                }
                inode->i_size = 0;
            }
            if (inode->i_block != NULL) {
                if (i_block_free(inode->i_block) == -1) {
                    return -1;
                }
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }
    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    int res = add_to_open_file_table(inum, offset);
    return res;

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { 
    int res = remove_from_open_file_table(fhandle);
    return res; 
    }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL)
        return -1;

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL)
        return -1;

    /* Determine how many bytes to write */
    if (to_write + file->of_offset > ((MAX_DIRECT_REFS + MAX_SUPPL_REFS)* BLOCK_SIZE))
        to_write = ((MAX_DIRECT_REFS + MAX_SUPPL_REFS)* BLOCK_SIZE) - file->of_offset;

    if (to_write > 0 && inode->i_block == NULL && inode->i_size + to_write > MAX_DIRECT_REFS * BLOCK_SIZE) {
        /* If it exceeds direct blocks size, allocate the block */
        inode->i_block = i_block_alloc();
        if (inode->i_block == NULL)
            return -1;
    }

    int block_index = (int)(inode->i_size / BLOCK_SIZE);
    char *buffer_pos = (char*)buffer;
    size_t left_to_write = to_write;
    pthread_rwlock_wrlock(&rwlock);
    /* Writing in the data blocks */
    while (block_index < MAX_DIRECT_REFS) {
        char* block = data_block_get(inode->i_data_blocks[block_index]);
        char* block_pos = block + file->of_offset%BLOCK_SIZE;
        long unsigned int j = 0;
        while (j < BLOCK_SIZE-file->of_offset%BLOCK_SIZE && left_to_write > 0) {
            block_pos[j] = buffer_pos[j];
            left_to_write--;
            j++;
        }
        file->of_offset += j;
        if (j == BLOCK_SIZE-file->of_offset%BLOCK_SIZE)
            block_index++;
        if (left_to_write == 0)
            goto end;
    }
    while (block_index < MAX_SUPPL_REFS) {
        char* block = data_block_get(inode->i_block->indexes[block_index-MAX_DIRECT_REFS]);
        char* block_pos = block + file->of_offset%BLOCK_SIZE;
        long unsigned int j = 0;
        while (j < BLOCK_SIZE-file->of_offset%BLOCK_SIZE && left_to_write > 0) {
            block_pos[j] = buffer_pos[j];
            left_to_write--;
            j++;
        }
        file->of_offset += j;
        if (j == BLOCK_SIZE-file->of_offset%BLOCK_SIZE)
            block_index++;
        if (left_to_write == 0)
            goto end;
    }
    end:
    if (file->of_offset > inode->i_size)
        inode->i_size = file->of_offset;
    pthread_rwlock_unlock(&rwlock);
    return (ssize_t)to_write;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL)
        return -1;

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL)
        return -1;

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len)
        to_read = len;

    int first_block = (int) file->of_offset / BLOCK_SIZE;

    size_t pre_trunc = file->of_offset % BLOCK_SIZE;
    size_t post_trunc = to_read % BLOCK_SIZE;
    size_t write_amount;
    pthread_rwlock_wrlock(&rwlock);
    if (to_read > 0) {
        int read_times = (int )(to_read / BLOCK_SIZE) + 1;
        for (int i = first_block, j = 0; len > 0 && i < first_block + read_times; i++, j++) {
            void* block = (i < MAX_DIRECT_REFS) ?
            data_block_get(inode->i_data_blocks[i]) : 
            i_block_get(i-MAX_DIRECT_REFS, inode->i_block);
            if (block == NULL) {
                return -1;
            }
            if (i == first_block) { // copy from the first block, from the cursor position, to the beginning of the buffer.
                write_amount = (BLOCK_SIZE - pre_trunc > len) ? len : BLOCK_SIZE - pre_trunc;
                memcpy(buffer, block + pre_trunc, write_amount);
            }
            else if (i < read_times - 1) {  // copy a BLOCK_SIZE from the i_th block to the i_th portion of the buffer. 
                write_amount = (BLOCK_SIZE - pre_trunc > len) ? len : BLOCK_SIZE;
                memcpy(buffer + j * BLOCK_SIZE, block, write_amount);
            }
            else {  // copy from the last block until the EOF to the last BLOCK_SIZE'd portion of the buffer.
                write_amount = (BLOCK_SIZE - pre_trunc > len) ? len : BLOCK_SIZE - post_trunc;
                memcpy(buffer + j * BLOCK_SIZE, block, write_amount);
            }
            len -= write_amount;
        }
        /*
        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            return -1;
        }

        Acho que isto não é preciso lol, 
        pelo menos não faz sentido que a leitura altere a posição do cursor

        Perform the actual read
            memcpy(buffer, block + file->of_offset, to_read);
        The offset associated with the file handle is
        incremented accordingly
            file->of_offset += to_read;*/
    }
    pthread_rwlock_unlock(&rwlock);
    return (ssize_t)to_read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    FILE *fp = fopen(dest_path, "w");
    int inum = tfs_lookup(source_path);
    inode_t *inode = inode_get(inum);
    if (fp == NULL || inode == NULL)
        return -1;
    
    for (int i = 0; i < MAX_DIRECT_REFS; i++) {
        char* block = data_block_get(inode->i_data_blocks[i]);
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (!block[j])
                goto end;
            fprintf(fp, "%c", block[j]);
        }
    }
    for (int i = 0; i < MAX_SUPPL_REFS; i++) {
        char* block = data_block_get(inode->i_block->indexes[i]);
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (!block[j])
                goto end;
            fprintf(fp, "%c", block[j]);
        }
    }
    end:
    fclose(fp);
    return 0;
}