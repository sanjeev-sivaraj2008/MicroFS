#include "../include/fs.h"
#include <stdio.h>
#include <assert.h>

void test_disk() {
    // fresh disk
    assert(disk_init("test.img") == FS_OK);
    
    // write and read back
    uint8_t wbuf[FS_BLOCK_SIZE] = {0};
    wbuf[0] = 42;
    assert(disk_write_block(5, wbuf) == FS_OK);
    
    uint8_t rbuf[FS_BLOCK_SIZE] = {0};
    assert(disk_read_block(5, rbuf) == FS_OK);
    assert(rbuf[0] == 42);
    
    // bad args
    assert(disk_write_block(99999, wbuf) == FS_ERR_BAD_ARG);
    assert(disk_read_block(5, NULL) == FS_ERR_BAD_ARG);
    
    disk_close();
    printf("disk tests passed\n");
}

void test_bitmap() {
    assert(disk_init("test.img") == FS_OK);
    
    // alloc a block
    int b1 = bitmap_alloc();
    assert(b1 >= FS_DATA_START);
    assert(bitmap_is_free(b1) == 0);  // now used
    
    // alloc another
    int b2 = bitmap_alloc();
    assert(b2 == b1 + 1);  // should be next block
    
    // free b1
    assert(bitmap_free(b1) == FS_OK);
    assert(bitmap_is_free(b1) == 1);  // free again
    
    // bad args
    assert(bitmap_is_free(5) == FS_ERR_BAD_ARG);  // below DATA_START
    
    disk_close();
    printf("bitmap tests passed\n");
}

int main() {
    test_disk();
    test_bitmap();
    return 0;
}