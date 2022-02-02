#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILDES_LEN (2) // TODO talvez mudar isto para client api
#define MAX_FILE_NAME (40)
#define MAX_PATH_NAME (40) //
#define MAX_SESSIONS (10) //
#define MAX_SESSION_ID_LEN (1) //
#define MAX_REQUEST_SIZE (2000) //

#define DELAY (5000)

#endif // CONFIG_H
