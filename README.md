# MicroFS — Task 2: Core File System

> This README lives at the root of the repo. Read it fully before you touch any code.

---

## What you're building

MicroFS is a working file system that lives entirely inside a single 4MB binary file called `disk.bin`. It has inodes, directories, path resolution, and byte-level file I/O — the same concepts that power ext4 and NTFS, just small enough to hold in your head.

At the end of Task 2, the four of you will have built a file system that can:

- Format a blank disk
- Create, read, write, and delete files
- Organise files into directories and subdirectories
- Navigate using full paths like `/home/user/notes.txt`

---

## Project structure

```
microfs/
│
├── include/
│   └── fs.h                ← shared interface — mentor-owned, do not change without discussion
│
├── src/
│   ├── disk.c              ← block read/write, disk open/close
│   ├── bitmap.c            ← free block tracking
│   ├── inode.c             ← inode alloc/free/read/write
│   ├── file.c              ← file create/read/write/delete
│   ├── dir.c               ← directory entries, mkdir, ls
│   ├── path.c              ← path resolution
│   ├── journal.c           ← write-ahead log — leave empty for now
│   ├── recovery.c          ← crash recovery — leave empty for now
│   └── cache.c             ← buffer cache — leave empty for now
│
├── cli/
│   └── shell.c             ← interactive CLI — leave empty for now
│
├── bench/
│   └── benchmark.c         ← throughput benchmarks — leave empty for now
│
├── tests/
│   ├── test_disk.c         ← write alongside disk.c
│   ├── test_inode.c        ← write alongside inode.c
│   ├── test_file.c         ← write alongside file.c
│   ├── test_dir.c          ← write alongside dir.c
│   └── test_integration.c  ← written together at the integration checkpoint
│
├── docs/
│   └── design.md           ← design document — leave empty for now
│
├── task1/                  ← your individual learning task — do not modify
├── Makefile                ← build system
├── .gitignore              ← disk.bin and *.o are gitignored — never commit them
└── README.md               ← this file
```

### How the layers stack

Every module depends on the one below it. This is the dependency order:

```
┌─────────────────────────────────────┐
│        Directories + Path           │  
├─────────────────────────────────────┤
│        File Operations              │  
├──────────────────┬──────────────────┤
│       Inodes     │                  │ 
├──────────────────┤        Disk      │  
│                  │                  │
└──────────────────┴──────────────────┘
```

---

## Disk layout

Your 4MB `disk.bin` is divided into fixed regions. Every module needs to know this.

```
┌─────────────────────────────────────────────────────────┐
│  Block 0          │  Superblock                         │
│  Blocks  1–8      │  Inode table  (128 inodes)          │
│  Block   9        │  Block bitmap                       │
│  Blocks 10–41     │  Journal region  (Task 3)           │
│  Blocks 42+       │  Data blocks  (file content lives here) │
└─────────────────────────────────────────────────────────┘
```

One block = **512 bytes**. Total = 8192 blocks = 4MB.

The byte offset of any block on disk: `offset = block_number × 512`

The data region starts at block 42. When the bitmap allocates a free block, it will always return a number ≥ 42.

---

## The shared contract — `include/fs.h`

This file is committed to main before you branch. It declares every function signature, every constant, and every struct that all four modules share.

**The rule:** You do not change anything in `fs.h` without raising it with the group first. If a signature needs to change, open a GitHub issue → discuss → mentor updates `fs.h` on main → everyone rebases. This is how real engineering teams manage shared APIs.

Read `fs.h` completely before writing any code. It tells you exactly what to implement.

---

## Module ownership

| Module | Branch | Files to create | Depends on |
|--------|--------|----------------|------------|
| Disk layer | `task2/disk` | `src/disk.c`, `src/bitmap.c`, `tests/test_disk.c` |
| Inode table | `task2/inodes` | `src/inode.c`, `tests/test_inode.c` |
| File operations | `task2/files` | `src/file.c`, `tests/test_file.c` |
| Dirs + paths | `task2/dirs` | `src/dir.c`, `src/path.c`, `tests/test_dir.c` |


---

## Module details

### Disk layer (`src/disk.c`, `src/bitmap.c`)

You are the foundation. Every other module calls your functions to touch the disk. Nobody else calls `fread` or `fwrite` directly — ever.

**What to implement:**

`disk_init(path)` — open the virtual disk file. If it doesn't exist, create it and zero-fill to exactly 4MB.

`disk_read_block(block_num, buf)` — seek to `block_num × 512`, read exactly 512 bytes into `buf`.

`disk_write_block(block_num, buf)` — seek to `block_num × 512`, write exactly 512 bytes from `buf`.

`bitmap_alloc()` — read block 9 (the bitmap block), find the first 0 bit, set it to 1, write block 9 back, return the block number. The returned number must always be ≥ 42.

`bitmap_free(block_num)` — read block 9, clear the bit for `block_num`, write back.

`bitmap_is_free(block_num)` — return 1 if the bit is clear, 0 if set.

**Test coverage required (`tests/test_disk.c`):**
- Write a known byte pattern to block 42, read it back, verify byte-for-byte
- Allocate 10 blocks via bitmap, verify all are unique and ≥ 42
- Free 3 of them, verify they show as free in the bitmap
- Verify `bitmap_alloc()` returns `FS_ERR_NO_SPACE` when the disk is full

**The silent killer:** The seek offset formula. `disk_read_block(5, buf)` must seek to byte **2560**, not byte 5. Verify this with `xxd disk.bin`

---

### Inode table (`src/inode.c`)

An inode is a 32-byte struct (defined in `fs.h`) that describes one file or directory. There are 128 inode slots on disk, stored in blocks 1–8.

**What to implement:**

`inode_alloc()` — scan all 128 slots, find one with `in_use == 0`, mark it used, zero all other fields, write to disk, return the inode number.

`inode_free(ino)` — zero out the entire inode slot and write it back. Do not free data blocks — that is the caller's responsibility.

`inode_read(ino, out)` — calculate the disk position of inode `ino`, read the 32-byte chunk into `*out`.

`inode_write(ino, in)` — calculate the disk position, read the block, overwrite the 32 bytes, write the block back.

`inode_get_block(ino, idx)` — given logical block index `idx` within a file, return the physical disk block number. For idx < 8: return `inode.blocks[idx]`. For idx ≥ 8: read `inode.indirect` (a block full of `uint32_t` block numbers) and return entry `idx - 8`.

`inode_append_block(ino)` — call `bitmap_alloc()`, attach the new block to the inode (filling direct slots first, then the indirect block), write the updated inode, return the new physical block number.

**The math:** Inode `n` is at byte offset `512 + (n × 32)` from the start of the disk. That means it lives in block `1 + (n × 32) / 512`, at byte `(n × 32) % 512` within that block.

**Test coverage required (`tests/test_inode.c`):**
- Allocate an inode, append 10 blocks to it (this crosses into the indirect block after slot 7), read back and verify all 10 block numbers are non-zero and unique
- Free the inode, verify the slot is zeroed

---

### File operations (`src/file.c`)

You bridge the gap between "give me bytes 200–600 of this file" and the block world underneath.

**What to implement:**

`file_create(name, parent_ino)` — allocate a fresh inode, set `is_dir = 0`, call `dir_add_entry(parent_ino, name, new_ino)` to register it in the parent directory. Return the new inode number.

`file_read(ino, buf, len, offset)` — calculate which block `offset` falls into (`block_idx = offset / 512`), use `inode_get_block` to find the physical block, read it, copy the right bytes into `buf`. Loop if the read spans multiple blocks. Stop at EOF.

`file_write(ino, buf, len, offset)` — same arithmetic in reverse. Call `inode_append_block` when writing past the end of the file. Update `inode.size` if the file grew.

`file_delete(name, parent_ino)` — call `dir_remove_entry` first (unlink from directory), then `bitmap_free` all data blocks, then `inode_free`. Order matters.

**The block crossing problem:** A write of 1100 bytes starting at offset 0 fills block 0 completely (512 bytes), overflows into block 1 completely (512 bytes), and writes 76 bytes into block 2. Each block is a separate `disk_read_block → modify → disk_write_block` cycle. Draw this on paper before coding.

**Test coverage required (`tests/test_file.c`):**
- Write 1100 bytes to a file, read it back fully and verify
- Read 200 bytes starting at offset 600 and verify
- Delete the file, verify its inode is free and its blocks are clear in the bitmap

---

### Directories + path resolution (`src/dir.c`, `src/path.c`)

A directory is an inode with `is_dir = 1` whose data blocks hold a flat array of `DirEntry` structs (defined in `fs.h`). Each entry is 32 bytes: a 4-byte inode number and a 28-byte null-terminated name. You read and write these using `file_read` and `file_write`.

**What to implement:**

`dir_init_root()` — allocate inode 0, set `is_dir = 1`, add two entries: `.` → inode 0, `..` → inode 0. Called once by `fs_mkfs`. If this is wrong, everything else fails.

`dir_mkdir(name, parent_ino)` — allocate a new inode, set `is_dir = 1`, add `.` and `..` entries, add the new directory to the parent.

`dir_lookup(dir_ino, name)` — read the directory's data, scan each `DirEntry`, return the inode number of the matching name or `FS_ERR_NOT_FOUND`.

`dir_add_entry(dir_ino, name, ino)` — find a slot with `ino == 0`, write the new entry. If no empty slot, call `inode_append_block` to get a new data block for the directory.

`dir_remove_entry(dir_ino, name)` — find the entry, zero out its `ino` and `name` fields, write back.

`dir_ls(dir_ino)` — print all non-empty entries to stdout with their type and size.

`path_resolve(path)` — split `path` on `/`, start at inode 0, call `dir_lookup` on each component. Return the final inode number.

`path_resolve_parent(path, name_out)` — same but stop one component early. Write the final component into `name_out`. Used by `file_create` and `file_delete`.

**Test coverage required (`tests/test_dir.c`):**
- Create `/home/`, create `/home/user/`, create a file inside, call `dir_ls`, resolve the full path, delete the file and verify lookup fails.

---

## Integration checkpoint

Once all four PRs are merged to main, the whole group writes `tests/test_integration.c` together in one session. It must:

1. Format a fresh disk with `fs_mkfs("disk.bin")`
2. Create the directory tree `/home/user/`
3. Write a 2KB file to `/home/user/notes.txt`
4. Read it back and verify it matches
5. Create three more files in different directories
6. Delete two of them and verify they're gone
7. Call `fs_fsck()` and confirm zero errors

If this passes — Task 2 is complete.

---

## Git rules

```bash
# Keep your fork up to date
git fetch upstream
git rebase upstream/main

# Never push directly to main
# Never commit disk.bin or *.o files — they are gitignored

# PR descriptions must include:
# - What you implemented
# - How to test it
# - Any known limitations
```

---

## Build

```bash
make              # build all modules
make test         # run all tests
make clean        # remove build artifacts
```