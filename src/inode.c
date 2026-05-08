#include<stdio.h>
#include "../include/fs.h"
#include<string.h>
/*
 * M2 — INODE TABLE                            src/inode.c
 * Manages the fixed-size table of inodes stored on disk.
 * */

/*
 * inode_alloc — find a free inode slot, mark it in_use, zero all fields.
 * Returns: inode number (0 to FS_MAX_INODES-1) on success, FS_ERR_NO_SPACE
 */
int inode_alloc(void)
{
    Inode inode;
    for(int ino=0;ino<FS_MAX_INODES;ino++)

    {
        int block=ino/16+FS_INODE_START ; //16 inodes in each block
        int offset=(ino%16)*sizeof(Inode);//offset of the inode at each block
        uint8_t buf[FS_BLOCK_SIZE];
        disk_read_block(block,buf);
        memcpy(&inode,buf+offset,sizeof(Inode));
        if(inode.in_use==0)
        {
            memset(&inode,0,sizeof(Inode));
            inode.in_use=1;
            memcpy(buf+offset,&inode,sizeof(Inode));
            disk_write_block(block,buf);
            return ino;
        }

    }
    return FS_ERR_NO_SPACE;

}

/*
 * inode_free — mark inode `ino` as free and zero its fields on disk.
 * Does NOT free the data blocks — caller must do that first.
 * Returns: FS_OK, FS_ERR_BAD_ARG
 */
int inode_free(int ino)
{
    if(ino<0 || ino>=FS_MAX_INODES)
    return FS_ERR_BAD_ARG;

    Inode inode;
    int block=(ino)/16+FS_INODE_START;
    int offset=(ino%16)*sizeof(Inode);
    uint8_t buf[FS_BLOCK_SIZE];
    disk_read_block(block,buf);
    memcpy(&inode,buf+offset,sizeof(Inode));
    memset(&inode,0,sizeof(Inode));
    memcpy(buf+offset,&inode,sizeof(Inode));
    disk_write_block(block,buf);
    return FS_OK;

}

/*
 * inode_read — load inode `ino` from disk into the struct at `*out`.
 * Returns: FS_OK, FS_ERR_BAD_ARG, FS_ERR_IO
 */
int inode_read(int ino, Inode *out)
{
    if(ino<0 || ino>=FS_MAX_INODES)
    return FS_ERR_BAD_ARG;

    int block = (ino)/16+FS_INODE_START;
    int offset = (ino%16)*sizeof(Inode);

    uint8_t buf[FS_BLOCK_SIZE];
    if(disk_read_block(block,buf)!=FS_OK)
    return FS_ERR_IO;
    memcpy(out,buf+offset,sizeof(Inode));
    return FS_OK;
}

/*
 * inode_write — persist the struct at `*in` to inode slot `ino` on disk.
 * Returns: FS_OK, FS_ERR_BAD_ARG, FS_ERR_IO
 */
int inode_write(int ino, const Inode *in)
{
     if(ino<0 || ino>=FS_MAX_INODES)
    return FS_ERR_BAD_ARG;

    int block = (ino)/16+FS_INODE_START;
    int offset = (ino%16)*sizeof(Inode);

    uint8_t buf[FS_BLOCK_SIZE];
    disk_read_block(block,buf);

    memcpy(buf+offset,in,sizeof(Inode));
    return disk_write_block(block,buf);
   
}

/*
 * inode_get_block — resolve logical block index `idx` within inode `ino`
 * to a physical block number. Handles both direct and indirect blocks.
 * Returns: physical block number on success, FS_ERR_BAD_ARG, FS_ERR_IO
 */
int inode_get_block(int ino, uint32_t idx)
{
      if(ino<0 || ino>=FS_MAX_INODES)
    return FS_ERR_BAD_ARG;

    int block = (ino)/16+FS_INODE_START;
    int offset = (ino%16)*sizeof(Inode);

    Inode inode;
    uint8_t buf[FS_BLOCK_SIZE];
    if(disk_read_block(block,buf)!=FS_OK)
    return FS_ERR_IO;
    memcpy(&inode,buf+offset,sizeof(Inode));
    if(idx>=0 && idx<FS_DIRECT_BLOCKS)
    {
        return inode.blocks[idx];
    }
    else
    {
        uint8_t buf[FS_BLOCK_SIZE];
        if(disk_read_block(inode.indirect,buf)!=FS_OK)
        return FS_ERR_IO;
        uint32_t *pointers=(uint32_t *)buf;
        return pointers[idx-FS_DIRECT_BLOCKS];
    }
    
    
}

/*
 * inode_append_block — allocate a new data block and attach it to
 * inode `ino` as its next logical block (direct or via indirect).
 * Returns: physical block number of the new block, FS_ERR_NO_SPACE, FS_ERR_IO
 */
int inode_append_block(int ino)
{
     if(ino<0 || ino>=FS_MAX_INODES)
    return FS_ERR_BAD_ARG;
   
   int block=bitmap_alloc();
   if(block<0)
   return FS_ERR_NO_SPACE;

     

    int ino_block = (ino)/16+FS_INODE_START;
    int offset = (ino%16)*sizeof(Inode);
    
    Inode inode;
    uint8_t buf[FS_BLOCK_SIZE];

    if(disk_read_block(ino_block,buf)!=FS_OK)
    return FS_ERR_IO;
    memcpy(&inode,buf+offset,sizeof(Inode));
    for(int idx=0;idx<FS_DIRECT_BLOCKS;idx++)
    {
        if(inode.blocks[idx]==0)
        {
            inode.blocks[idx]=block;
            memcpy(buf+offset,&inode,sizeof(Inode));
            if(disk_write_block(ino_block,buf)!=FS_OK)
            return FS_ERR_IO;
            return block;
        }
    }
    

    if(inode.indirect==0)
    {
        int ind_block=bitmap_alloc();
        if(ind_block<0)
        return FS_ERR_NO_SPACE;
        inode.indirect=ind_block;
         uint8_t ind[FS_BLOCK_SIZE];
        memset(ind,0,FS_BLOCK_SIZE);
        if(disk_write_block(inode.indirect,ind)!=FS_OK)
        return FS_ERR_IO;
        memcpy(buf+offset,&inode,sizeof(Inode));
       if(disk_write_block(ino_block,buf)!=FS_OK)
       return FS_ERR_IO;
    }
    
    

    uint8_t ind[FS_BLOCK_SIZE];
   if(disk_read_block(inode.indirect,ind)!=FS_OK)
   return FS_ERR_IO;

    uint32_t *pointers=(uint32_t *)ind;
    for(int idx=0;idx<FS_BLOCK_SIZE/4;idx++)
    {
        if(pointers[idx]==0)
        {
            pointers[idx]=block;
            if(disk_write_block(inode.indirect,pointers)!=FS_OK)
            return FS_ERR_IO;
            
            return block;
        }
    }
    return FS_ERR_NO_SPACE;

}
