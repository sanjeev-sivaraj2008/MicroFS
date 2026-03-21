# Task 1 — Flat File System (Learning Task)

> **Before you read this:** Make sure you've forked the repo, cloned your fork locally, and created your branch (`task1/<yourname>`) before writing a single line of code.

---

## What is this?

This is a solo warm-up task. You will build a tiny, stripped-down file system in C — no inodes, no directories, no bitmaps. Just a flat binary file that stores up to 8 files in fixed-size slots.

It exists for two reasons:

1. **Get comfortable with the exact C patterns** you'll use in the real project — binary file I/O, struct layout, byte manipulation, and Makefiles.
2. **Set up the Git workflow** — forking, branching, committing, and opening PRs — before the team depends on you doing it correctly.


---

## What you will build

A C program that manages a 1MB binary file called `disk.bin`. Inside this file you maintain a table of up to 8 file records. Each record holds a filename and up to 484 bytes of content.

### Three functions to implement

| Function | What it does |
|----------|-------------|
| `fs_create(name, data)` | Find a free slot, write the filename and data, save to disk |
| `fs_read(name)` | Scan for the filename, print its content |
| `fs_delete(name)` | Find the filename, zero out the slot, save to disk |

### Disk layout

Your `disk.bin` is a flat array of 8 slots. Each slot is exactly **512 bytes**:

```
┌─────────────────────────────────────────────┐
│  Bytes   0–23  │  filename (null-terminated) │
│  Byte    24    │  in_use flag (1=yes, 0=no)  │
│  Bytes  25–27  │  padding (zeroed)           │
│  Bytes 28–511  │  file content (max 484 B)   │
└─────────────────────────────────────────────┘
```

Define this as a C struct. Use `__attribute__((packed))` and verify `sizeof(Slot) == 512` with a compile-time assert.

---

## Files to create

```
task1/
├── disk.c      ← open/create disk.bin, read/write individual slots by index
├── fs.c        ← fs_create, fs_read, fs_delete
├── main.c      ← test driver that exercises all three functions
└── Makefile    ← supports: make · make test · make clean
```

No other files. Keep it simple.

---

## Git workflow

```bash
# 1. Fork the mentor's repo on GitHub 
# 2. Clone your fork
git clone https://github.com/<you>/microfs.git
cd microfs

# 3. Create your branch
git checkout -b task1/<yourname>

# 4. Work, commit as you go
git add task1/
git commit -m "implement fs_create with linear scan for free slot"

# 5. Push and open a PR to your own fork's main
git push origin task1/<yourname>
```

Later task shall be PR'd to both forked origin and upstream

**Commit message rule:** `"done"` is not a commit message. Describe what changed and why. Examples:
- `"add disk_open that creates disk.bin if missing"`
- `"fix fs_delete not zeroing the in_use flag"`

---

## Quick reference

```c
// Reading a slot from disk
FILE *f = fopen("disk.bin", "rb");
fseek(f, slot_index * 512, SEEK_SET);
fread(&slot, sizeof(Slot), 1, f);
fclose(f);

// Writing a slot to disk
FILE *f = fopen("disk.bin", "r+b");
fseek(f, slot_index * 512, SEEK_SET);
fwrite(&slot, sizeof(Slot), 1, f);
fclose(f);
```

Do not keep the file open across multiple operations in this task. Open, seek, read/write, close. Simple.