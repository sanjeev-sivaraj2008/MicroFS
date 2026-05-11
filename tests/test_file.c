#include "../include/fs.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// helper to set up a basic directory for testing
int setup_test_dir() {
    int ino = inode_alloc();
    Inode inode;
    inode_read(ino, &inode);
    inode.is_dir = 1;
    inode.size = 0;
    inode_write(ino, &inode);
    return ino;
}

void test_file_create() {
    int dir_ino = setup_test_dir();

    // basic create
    int ino = file_create("test.txt", dir_ino);
    assert(ino >= 0);

    // verify inode is set up correctly
    Inode inode;
    inode_read(ino, &inode);
    assert(inode.in_use == 1);
    assert(inode.is_dir == 0);
    assert(inode.size == 0);

    // duplicate name should fail
    assert(file_create("test.txt", dir_ino) == FS_ERR_EXISTS);

    // bad args
    assert(file_create(NULL, dir_ino) == FS_ERR_BAD_ARG);
    assert(file_create("test2.txt", -1) == FS_ERR_BAD_ARG);
    assert(file_create("test2.txt", FS_MAX_INODES) == FS_ERR_BAD_ARG);

    // parent not a directory
    assert(file_create("test2.txt", ino) == FS_ERR_NOT_DIR);

    printf("file_create tests passed\n");
}

void test_file_read_write() {
    int dir_ino = setup_test_dir();
    int ino = file_create("rw.txt", dir_ino);
    assert(ino >= 0);

    // write some data
    char *msg = "Hello MicroFS";
    int w = file_write(ino, msg, strlen(msg), 0);
    assert(w == (int)strlen(msg));

    // verify size updated
    Inode inode;
    inode_read(ino, &inode);
    assert(inode.size == strlen(msg));

    // read it back
    char rbuf[64];
    memset(rbuf, 0, sizeof(rbuf));
    int r = file_read(ino, rbuf, strlen(msg), 0);
    assert(r == (int)strlen(msg));
    assert(strcmp(rbuf, msg) == 0);

    // read with offset
    memset(rbuf, 0, sizeof(rbuf));
    r = file_read(ino, rbuf, 5, 6);
    assert(r == 5);
    assert(strncmp(rbuf, "MicroFS", 5) == 0);

    // read past EOF
    r = file_read(ino, rbuf, 100, inode.size);
    assert(r == 0);

    // write past current size (extend file)
    char *msg2 = " extended";
    w = file_write(ino, msg2, strlen(msg2), strlen(msg));
    assert(w == (int)strlen(msg2));
    inode_read(ino, &inode);
    assert(inode.size == strlen(msg) + strlen(msg2));

    // bad args
    assert(file_read(-1, rbuf, 10, 0) == FS_ERR_BAD_ARG);
    assert(file_read(ino, NULL, 10, 0) == FS_ERR_BAD_ARG);
    assert(file_write(-1, msg, 10, 0) == FS_ERR_BAD_ARG);
    assert(file_write(ino, NULL, 10, 0) == FS_ERR_BAD_ARG);

    printf("file_read/write tests passed\n");
}

void test_file_write_multiblock() {
    int dir_ino = setup_test_dir();
    int ino = file_create("big.txt", dir_ino);
    assert(ino >= 0);

    // write more than one block
    uint8_t wbuf[FS_BLOCK_SIZE * 3];
    memset(wbuf, 0xAB, sizeof(wbuf));
    int w = file_write(ino, wbuf, sizeof(wbuf), 0);
    assert(w == sizeof(wbuf));

    // read it back
    uint8_t rbuf[FS_BLOCK_SIZE * 3];
    memset(rbuf, 0, sizeof(rbuf));
    int r = file_read(ino, rbuf, sizeof(rbuf), 0);
    assert(r == sizeof(rbuf));
    assert(memcmp(wbuf, rbuf, sizeof(wbuf)) == 0);

    printf("file multiblock read/write tests passed\n");
}

void test_file_delete() {
    int dir_ino = setup_test_dir();
    int ino = file_create("del.txt", dir_ino);
    assert(ino >= 0);

    // write some data so blocks are allocated
    char *msg = "delete me";
    file_write(ino, msg, strlen(msg), 0);

    // delete it
    assert(file_delete("del.txt", dir_ino) == FS_OK);

    // inode should be free
    Inode inode;
    inode_read(ino, &inode);
    assert(inode.in_use == 0);

    // looking it up should fail
    assert(dir_lookup(dir_ino, "del.txt") == FS_ERR_NOT_FOUND);

    // delete again should fail
    assert(file_delete("del.txt", dir_ino) == FS_ERR_NOT_FOUND);

    // bad args
    assert(file_delete(NULL, dir_ino) == FS_ERR_BAD_ARG);
    assert(file_delete("del.txt", -1) == FS_ERR_BAD_ARG);

    // deleting a directory should fail
    int subdir = setup_test_dir();
    dir_add_entry(dir_ino, "subdir", subdir);
    assert(file_delete("subdir", dir_ino) == FS_ERR_IS_DIR);

    printf("file_delete tests passed\n");
}

int main() {
    assert(disk_init("test_file.img") == FS_OK);

    test_file_create();
    test_file_read_write();
    test_file_write_multiblock();
    test_file_delete();

    disk_close();
    printf("all file tests passed\n");
    return 0;
}