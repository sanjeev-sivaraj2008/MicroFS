#include "../include/fs.h"
#include<stdio.h>
#include<string.h>
/*
 * bitmap_alloc — find and mark the first free data block as used.
 * Returns: block number (>= FS_DATA_START) on success, FS_ERR_NO_SPACE
 */
int bitmap_alloc(void)
{
uint8_t buf[FS_BITMAP_BLOCKS*FS_BLOCK_SIZE];
disk_read_block(FS_BITMAP_START,buf);
disk_read_block(FS_BITMAP_START+1,buf+FS_BLOCK_SIZE);
for(int byte=0;byte<FS_BLOCK_SIZE*FS_BITMAP_BLOCKS;byte++)
{
    for(int bit=0;bit<8;bit++)
    {
        if(!(buf[byte] & (1<<bit)))//check if the bit is 0
        {
            buf[byte]=buf[byte] | (1<<bit); // change the bit to 1

            if(byte>=FS_BLOCK_SIZE)
            {
            disk_write_block(FS_BITMAP_START+1,buf+FS_BLOCK_SIZE);
           
            }
            else
            {
            disk_write_block(FS_BITMAP_START,buf);
            
            }
             return FS_DATA_START + byte*8 +bit;
        }
    }
    
}
return FS_ERR_NO_SPACE;


}

/*
 * bitmap_free — mark block `block_num` as free.
 * Returns: FS_OK, FS_ERR_BAD_ARG
 */
int bitmap_free(uint32_t block_num)
{
if(block_num > FS_TOTAL_BLOCKS || block_num < FS_DATA_START)
return FS_ERR_BAD_ARG;
else
{
uint8_t buf[FS_BITMAP_BLOCKS*FS_BLOCK_SIZE];
disk_read_block(FS_BITMAP_START,buf);
disk_read_block(FS_BITMAP_START+1,buf+FS_BLOCK_SIZE);
int byte=(block_num-FS_DATA_START)/8;
int bit=(block_num-FS_DATA_START)%8;
buf[byte]=buf[byte] & ~(1<<bit); // change bit to 0
if(byte>=FS_BLOCK_SIZE)
disk_write_block(FS_BITMAP_START+1,buf+FS_BLOCK_SIZE);
else
disk_write_block(FS_BITMAP_START,buf);
return FS_OK;
}

}

/*
 * bitmap_is_free — check whether a block is free.
 * Returns: 1 if free, 0 if used, FS_ERR_BAD_ARG if out of range
 */
int bitmap_is_free(uint32_t block_num)
{
if(block_num > FS_TOTAL_BLOCKS || block_num < FS_DATA_START)
return FS_ERR_BAD_ARG;
else
{
    uint8_t buf[FS_BITMAP_BLOCKS*FS_BLOCK_SIZE];
    disk_read_block(FS_BITMAP_START,buf);
    disk_read_block(FS_BITMAP_START+1,buf+FS_BLOCK_SIZE);
    int byte=(block_num-FS_DATA_START)/8;
    int bit=(block_num-FS_DATA_START)%8;
    if (buf[byte] & (1<<bit))
        return 0;
    else
        return 1;

}
}
