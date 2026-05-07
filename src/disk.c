#include "../include/fs.h"
#include<stdio.h>
#include<string.h>
FILE *f;

int disk_init(const char *path)
{
    f=fopen(path,"r+b");
    if(f!=NULL)
    {
    
        return FS_OK;

    }
    else
    {
        f=fopen(path,"w+b");
        if(f==NULL)
        return FS_ERR_IO;
        uint8_t buf[FS_BLOCK_SIZE]={0};
        for(int i=0;i<FS_TOTAL_BLOCKS;i++)
        {
            if(fwrite(buf,FS_BLOCK_SIZE,1,f)!=1)
            {
                fclose(f);
                return FS_ERR_IO;
            }
        }
    
        
        
        
        return FS_OK;
        
    }

}


/*
 * disk_close — flush and close the virtual disk file.
 * After this call, no other fs_* functions may be used until
 * disk_init() is called again.
 */
 void disk_close()
    {
        fclose(f);
        f=NULL;
    }


/*
 * disk_read_block — read one 512-byte block from the disk into buf.
 * `buf` must point to at least FS_BLOCK_SIZE bytes of writable memory.
 * Returns: FS_OK, FS_ERR_IO, FS_ERR_BAD_ARG
 */
int disk_read_block(uint32_t block_num, void *buf)
{
if(block_num>FS_TOTAL_BLOCKS || buf==NULL)
return FS_ERR_BAD_ARG;

if(fseek(f,block_num*512,SEEK_SET) != 0)
return FS_ERR_IO;

if(fread(buf,FS_BLOCK_SIZE,1,f) !=1)
return FS_ERR_IO;

return FS_OK;

}

/*
 * disk_write_block — write one 512-byte block from buf to the disk.
 * `buf` must point to at least FS_BLOCK_SIZE bytes of readable memory.
 * Returns: FS_OK, FS_ERR_IO, FS_ERR_BAD_ARG
 */
int disk_write_block(uint32_t block_num, const void *buf)
{
    if(block_num>FS_TOTAL_BLOCKS || buf==NULL)
    return FS_ERR_BAD_ARG;

    if(f==NULL)
    return FS_ERR_IO;

    if(fseek(f,block_num*FS_BLOCK_SIZE,SEEK_SET) !=0)
    return FS_ERR_IO;

    if(fwrite(buf,FS_BLOCK_SIZE,1,f) !=1)
    return FS_ERR_IO;

    return FS_OK;
}
