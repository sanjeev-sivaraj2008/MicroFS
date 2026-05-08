#include "../include/fs.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_inode_alloc_free() {
    // alloc first inode
    int ino = inode_alloc();
    assert(ino >= 0 && ino < FS_MAX_INODES);
    
    // alloc second, should be different
    int ino2 = inode_alloc();
    assert(ino2 != ino);
    
    // free first, alloc again should give same slot back
    inode_free(ino);
    int ino3 = inode_alloc();
    assert(ino3 == ino);
    
    // bad args
    assert(inode_free(-1) == FS_ERR_BAD_ARG);
    assert(inode_free(FS_MAX_INODES) == FS_ERR_BAD_ARG);
    
    printf("inode_alloc/free tests passed\n");
}

void test_inode_read_write() {
    int ino = inode_alloc();
    assert(ino >= 0);
    
    // read it back
    Inode inode;
    assert(inode_read(ino, &inode) == FS_OK);
    assert(inode.in_use == 1);
    assert(inode.size == 0);
    
    // modify and write
    inode.size = 100;
    inode.is_dir = 0;
    assert(inode_write(ino, &inode) == FS_OK);
    
    // read back and verify
    Inode inode2;
    assert(inode_read(ino, &inode2) == FS_OK);
    assert(inode2.size == 100);
    assert(inode2.is_dir == 0);
    
    // bad args
    assert(inode_read(-1, &inode) == FS_ERR_BAD_ARG);
    assert(inode_read(FS_MAX_INODES, &inode) == FS_ERR_BAD_ARG);
    assert(inode_write(-1, &inode) == FS_ERR_BAD_ARG);
    
    printf("inode_read/write tests passed\n");
}

void test_inode_append_get_block() {
    int ino = inode_alloc();
    assert(ino >= 0);
    
    // append first block
    int b0 = inode_append_block(ino);
    assert(b0 >= FS_DATA_START);
    
    // get it back
    assert(inode_get_block(ino, 0) == b0);
    
    // append until we fill direct blocks
    int blocks[FS_DIRECT_BLOCKS];
    blocks[0] = b0;
    for(int i = 1; i < FS_DIRECT_BLOCKS; i++) {
        blocks[i] = inode_append_block(ino);
        assert(blocks[i] >= FS_DATA_START);
        assert(inode_get_block(ino, i) == blocks[i]);
    }
    
    // append one more — goes into indirect
    int ind_block = inode_append_block(ino);
    assert(ind_block >= FS_DATA_START);
    assert(inode_get_block(ino, FS_DIRECT_BLOCKS) == ind_block);
    
    // bad args
    assert(inode_get_block(-1, 0) == FS_ERR_BAD_ARG);
    assert(inode_append_block(-1) == FS_ERR_BAD_ARG);
    
    printf("inode_append/get_block tests passed\n");
}

int main() {
    // fresh disk for each run
    assert(disk_init("test_inode.img") == FS_OK);
    
    test_inode_alloc_free();
    test_inode_read_write();
    test_inode_append_get_block();
    
    disk_close();
    printf("all inode tests passed\n");
    return 0;
}