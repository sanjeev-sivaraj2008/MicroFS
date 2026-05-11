/*
 * fs.h — MicroFS shared contract
 *
 * Implement the functions declared here in their respective src/ files.
 *
 * RULE: No student changes a signature or constant without a group discussion
 *       and a PR to main that everyone reviews. Changing this file mid-task
 *       breaks every other module that depends on it.
 *
 * Ownership map:
 *   disk_*       → src/disk.c
 *   bitmap_*     → src/bitmap.c
 *   inode_*      → src/inode.c
 *   file_*       → src/file.c
 *   dir_*        → src/dir.c
 *   path_*       → src/path.c
 *   cache_*      → src/cache.c
 *   journal_*    → src/journal.c
 *   fs_recover() → src/recovery.c
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#define FS_BLOCK_SIZE        512        /* bytes per block                  */
#define FS_TOTAL_BLOCKS      8192       /* 8192 × 512 = 4 MB virtual disk   */
#define FS_MAX_INODES        128        /* max files + directories           */
#define FS_MAX_FILENAME      27         /* max chars in a name (+ 1 null)   */
#define FS_DIRECT_BLOCKS     8          /* direct block pointers per inode   */
#define FS_CACHE_CAPACITY    16         /* LRU cache slots (Task 3, M6)      */
#define FS_JOURNAL_BLOCKS    32         /* blocks reserved for journal       */
#define FS_ROOT_INODE        0          /* inode number of root directory    */

/* Block layout on disk (regions, in order):
 *
 *   [ Superblock ][ Inode table ][ Bitmap ][ Journal ][ Data blocks ]
 *       1 block     8 blocks       2 block   32 blocks   rest
 *
 *   Block 0       : superblock
 *   Blocks 1–8    : inode table  (128 inodes × 32 bytes each = 4096 bytes = 8 blocks)
 *   Block 9-10       : block bitmap (one bit per data block)
 *   Blocks 11–42  : journal      (FS_JOURNAL_BLOCKS)
 *   Blocks 43+    : data blocks
 */
#define FS_SUPERBLOCK_BLOCK  0
#define FS_INODE_START       1
#define FS_INODE_BLOCKS      8
#define FS_BITMAP_START      9
#define FS_BITMAP_BLOCKS     2
#define FS_JOURNAL_START     11
#define FS_DATA_START        (FS_JOURNAL_START + FS_JOURNAL_BLOCKS)  /* = 43 */

/*
 * RETURN CODES
 * All functions return 0 on success or a negative FS_ERR_* on failure.
 * */

#define FS_OK                0
#define FS_ERR_IO           -1   /* disk read/write failed                  */
#define FS_ERR_NO_SPACE     -2   /* no free blocks or inodes                */
#define FS_ERR_NOT_FOUND    -3   /* file/dir name not found                 */
#define FS_ERR_EXISTS       -4   /* name already exists in directory        */
#define FS_ERR_NOT_DIR      -5   /* expected a directory, got a file        */
#define FS_ERR_IS_DIR       -6   /* expected a file, got a directory        */
#define FS_ERR_BAD_ARG      -7   /* null pointer, out-of-range inode, etc.  */
#define FS_ERR_CORRUPT      -8   /* magic number mismatch or bad checksum   */
#define FS_ERR_NOT_OPEN     -9   /* disk not initialised before operation   */

/*
 * ON-DISK STRUCTURES
 * These are written directly to disk — never add pointer fields.
 * Sizes must stay fixed: any change breaks existing disk images.
 * */

/*
 * Superblock — always at block 0.
 * Holds global metadata about the filesystem.
 */
typedef struct {
    uint32_t magic;            /* must equal FS_MAGIC (0xDEADC0DE)         */
    uint32_t version;          /* filesystem version, currently 1           */
    uint32_t total_blocks;     /* total blocks on disk                      */
    uint32_t data_start;       /* first usable data block number            */
    uint32_t inode_count;      /* total inode slots (FS_MAX_INODES)         */
    uint32_t free_blocks;      /* count of free data blocks                 */
    uint32_t free_inodes;      /* count of free inode slots                 */
    uint8_t  _pad[484];        /* pad to exactly 512 bytes (1 block)        */
} __attribute__((packed)) Superblock;

#define FS_MAGIC  0xDEADC0DE

/*
 * Inode — represents one file or directory.
 * 32 bytes each; 128 inodes fit in 8 blocks (FS_INODE_BLOCKS).
 *
 * blocks[0..7] : direct block numbers (0 = unused slot)
 * indirect     : block number of a 512-byte block holding up to 128
 *                additional block numbers (uint32_t each). Use only
 *                when direct blocks are exhausted.
 */
typedef struct {
    uint32_t size;                      /* file size in bytes                */
    uint32_t blocks[FS_DIRECT_BLOCKS];  /* direct data block numbers         */
    uint32_t indirect;                  /* indirect block (0 = none)         */
    uint8_t  is_dir;                    /* 1 = directory, 0 = regular file   */
    uint8_t  in_use;                    /* 1 = allocated, 0 = free slot      */
    uint8_t  _pad[2];                   /* align to 4 bytes                  */
} __attribute__((packed)) Inode;        /* exactly 32 bytes                  */

/*
 * DirEntry — one entry inside a directory's data blocks.
 * A directory's data is a flat array of DirEntry values.
 * Unused slots have ino == 0.
 */
typedef struct {
    uint32_t ino;                       /* inode number (0 = empty slot)     */
    char     name[FS_MAX_FILENAME + 1]; /* null-terminated filename          */
} __attribute__((packed)) DirEntry;     /* exactly 32 bytes                  */

/*
 * JournalEntry — one record in the write-ahead log (Task 3, M5).
 * Before any metadata write, log the intent here first.
 */
typedef struct {
    uint32_t magic;            /* FS_JOURNAL_MAGIC to detect partial writes  */
    uint32_t block_num;        /* which block is about to be written         */
    uint8_t  data[FS_BLOCK_SIZE]; /* the intended new contents of that block */
    uint32_t checksum;         /* simple XOR checksum of data[]              */
} __attribute__((packed)) JournalEntry;

#define FS_JOURNAL_MAGIC  0xC0FFEE42

/*
 * M1 — DISK LAYER                              src/disk.c
 * Low-level block I/O. Every other module calls only these two
 * functions to touch the disk — never fread/fwrite directly.
 * */

/*
 * disk_init — open (or create) the virtual disk file at `path`.
 * If the file does not exist, create it and zero-fill to 4 MB.
 * Must be called before any other fs_* function.
 * Returns: FS_OK, FS_ERR_IO
 */
int disk_init(const char *path);

/*
 * disk_close — flush and close the virtual disk file.
 * After this call, no other fs_* functions may be used until
 * disk_init() is called again.
 */
void disk_close(void);

/*
 * disk_read_block — read one 512-byte block from the disk into buf.
 * `buf` must point to at least FS_BLOCK_SIZE bytes of writable memory.
 * Returns: FS_OK, FS_ERR_IO, FS_ERR_BAD_ARG
 */
int disk_read_block(uint32_t block_num, void *buf);

/*
 * disk_write_block — write one 512-byte block from buf to the disk.
 * `buf` must point to at least FS_BLOCK_SIZE bytes of readable memory.
 * Returns: FS_OK, FS_ERR_IO, FS_ERR_BAD_ARG
 */
int disk_write_block(uint32_t block_num, const void *buf);

/*
 * M1 — BITMAP                                 src/bitmap.c
 * Tracks which data blocks are free or in use.
 * One bit per data block, stored at FS_BITMAP_BLOCK on disk.
 * */

/*
 * bitmap_alloc — find and mark the first free data block as used.
 * Returns: block number (>= FS_DATA_START) on success, FS_ERR_NO_SPACE
 */
int bitmap_alloc(void);

/*
 * bitmap_free — mark block `block_num` as free.
 * Returns: FS_OK, FS_ERR_BAD_ARG
 */
int bitmap_free(uint32_t block_num);

/*
 * bitmap_is_free — check whether a block is free.
 * Returns: 1 if free, 0 if used, FS_ERR_BAD_ARG if out of range
 */
int bitmap_is_free(uint32_t block_num);

/*
 * M2 — INODE TABLE                            src/inode.c
 * Manages the fixed-size table of inodes stored on disk.
 * */

/*
 * inode_alloc — find a free inode slot, mark it in_use, zero all fields.
 * Returns: inode number (0 to FS_MAX_INODES-1) on success, FS_ERR_NO_SPACE
 */
int inode_alloc(void);

/*
 * inode_free — mark inode `ino` as free and zero its fields on disk.
 * Does NOT free the data blocks — caller must do that first.
 * Returns: FS_OK, FS_ERR_BAD_ARG
 */
int inode_free(int ino);

/*
 * inode_read — load inode `ino` from disk into the struct at `*out`.
 * Returns: FS_OK, FS_ERR_BAD_ARG, FS_ERR_IO
 */
int inode_read(int ino, Inode *out);

/*
 * inode_write — persist the struct at `*in` to inode slot `ino` on disk.
 * Returns: FS_OK, FS_ERR_BAD_ARG, FS_ERR_IO
 */
int inode_write(int ino, const Inode *in);

/*
 * inode_get_block — resolve logical block index `idx` within inode `ino`
 * to a physical block number. Handles both direct and indirect blocks.
 * Returns: physical block number on success, FS_ERR_BAD_ARG, FS_ERR_IO
 */
int inode_get_block(int ino, uint32_t idx);

/*
 * inode_append_block — allocate a new data block and attach it to
 * inode `ino` as its next logical block (direct or via indirect).
 * Returns: physical block number of the new block, FS_ERR_NO_SPACE, FS_ERR_IO
 */
int inode_append_block(int ino);

/*
 * M3 — FILE OPERATIONS                        src/file.c
 * Byte-level read/write on regular files.
 * Depends on: disk_*, bitmap_*, inode_*
 * */

/*
 * file_create — create a new empty regular file named `name` inside
 * the directory identified by `parent_ino`.
 * Returns: inode number of the new file, FS_ERR_EXISTS, FS_ERR_NO_SPACE,
 *          FS_ERR_NOT_DIR, FS_ERR_BAD_ARG
 */
int file_create(const char *name, int parent_ino);

/*
 * file_read — read `len` bytes from inode `ino` starting at `offset`
 * into `buf`. Reads stop at EOF if offset+len exceeds file size.
 * Returns: number of bytes actually read (>= 0), or a negative FS_ERR_*
 */
int file_read(int ino, void *buf, uint32_t len, uint32_t offset);

/*
 * file_write — write `len` bytes from `buf` into inode `ino` starting
 * at `offset`. Extends the file if offset+len exceeds current size.
 * Returns: number of bytes actually written (>= 0), or a negative FS_ERR_*
 */
int file_write(int ino, const void *buf, uint32_t len, uint32_t offset);

/*
 * file_delete — remove the file named `name` from directory `parent_ino`,
 * free all its data blocks, and free its inode.
 * Returns: FS_OK, FS_ERR_NOT_FOUND, FS_ERR_IS_DIR, FS_ERR_BAD_ARG
 */
int file_delete(const char *name, int parent_ino);

/*
 * M4 — DIRECTORIES                            src/dir.c
 * A directory is an inode whose data blocks hold DirEntry arrays.
 * Depends on: disk_*, bitmap_*, inode_*, file_*
 * */

/*
 * dir_init_root — format and create the root directory (inode 0).
 * Called once by mkfs; adds "." and ".." entries pointing to itself.
 * Returns: FS_OK, FS_ERR_IO
 */
int dir_init_root(void);

/*
 * dir_mkdir — create a new subdirectory named `name` inside `parent_ino`.
 * Adds "." and ".." entries to the new directory automatically.
 * Returns: inode number of the new directory, FS_ERR_EXISTS,
 *          FS_ERR_NO_SPACE, FS_ERR_NOT_DIR, FS_ERR_BAD_ARG
 */
int dir_mkdir(const char *name, int parent_ino);

/*
 * dir_rmdir — remove an empty subdirectory named `name` from `parent_ino`.
 * Fails if the directory still contains entries other than "." and "..".
 * Returns: FS_OK, FS_ERR_NOT_FOUND, FS_ERR_NOT_DIR, FS_ERR_BAD_ARG
 */
int dir_rmdir(const char *name, int parent_ino);

/*
 * dir_lookup — search directory `dir_ino` for an entry named `name`.
 * Returns: inode number of the entry, FS_ERR_NOT_FOUND, FS_ERR_NOT_DIR
 */
int dir_lookup(int dir_ino, const char *name);

/*
 * dir_add_entry — add a (name → ino) entry to directory `dir_ino`.
 * Used internally by file_create and dir_mkdir.
 * Returns: FS_OK, FS_ERR_EXISTS, FS_ERR_NO_SPACE, FS_ERR_NOT_DIR
 */
int dir_add_entry(int dir_ino, const char *name, int ino);

/*
 * dir_remove_entry — remove the entry named `name` from directory `dir_ino`.
 * Used internally by file_delete and dir_rmdir.
 * Returns: FS_OK, FS_ERR_NOT_FOUND, FS_ERR_NOT_DIR
 */
int dir_remove_entry(int dir_ino, const char *name);

/*
 * dir_ls — print all entries in directory `dir_ino` to stdout.
 * Format: one "  <name>  [DIR|FILE]  <size> bytes" line per entry.
 * Returns: number of entries printed, or a negative FS_ERR_*
 */
int dir_ls(int dir_ino);

/*
 * M4 — PATH RESOLUTION                        src/path.c
 * Walks the directory tree to convert a path string to an inode.
 * */

/*
 * path_resolve — convert an absolute path (e.g. "/home/user/notes.txt")
 * to its inode number by walking the directory tree from root.
 * `path` must start with '/'.
 * Returns: inode number on success, FS_ERR_NOT_FOUND, FS_ERR_BAD_ARG
 */
int path_resolve(const char *path);

/*
 * path_resolve_parent — resolve the parent directory of `path` and
 * write the final component (filename/dirname) into `name_out`.
 * `name_out` must have room for at least FS_MAX_FILENAME+1 bytes.
 * Returns: inode number of parent dir, FS_ERR_NOT_FOUND, FS_ERR_BAD_ARG
 *
 * Example: path="/home/user/notes.txt"
 *          → returns inode of /home/user, name_out = "notes.txt"
 */
int path_resolve_parent(const char *path, char *name_out);

/*
 * HIGH-LEVEL FILESYSTEM LIFECYCLE
 * These two functions wrap everything together and are called by
 * the CLI later. They live in src/disk.c or a new fs_init.c.
 *

/*
 * fs_mkfs — format the virtual disk at `path` from scratch:
 *   1. disk_init()
 *   2. Write a fresh Superblock with correct magic and counts
 *   3. Zero the inode table and bitmap
 *   4. Zero the journal region
 *   5. dir_init_root()
 * Destroys all existing data on the disk.
 * Returns: FS_OK, FS_ERR_IO
 */
int fs_mkfs(const char *path);

/*
 * fs_mount — open an existing formatted disk at `path`.
 *   1. disk_init()
 *   2. Read and validate the Superblock magic number
 *   3. Replay any committed journal entries
 * Returns: FS_OK, FS_ERR_IO, FS_ERR_CORRUPT
 */
int fs_mount(const char *path);
#endif