#include<stdio.h>
#include<string.h>

#include "../include/fs.h"
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
int file_create(const char *name, int parent_ino)
{
    if(parent_ino>=FS_MAX_INODES || parent_ino<0 || name==NULL)
    return FS_ERR_BAD_ARG;  
    
    Inode inode;
    inode_read(parent_ino,&inode);
    if(inode.is_dir==0)
    return FS_ERR_NOT_DIR;

    if(dir_lookup(parent_ino,name)>=0)
    return FS_ERR_EXISTS;

    int ino = inode_alloc();
    if(ino<0)
    return ino;
    Inode new_inode;
    inode_read(ino,&new_inode);
    new_inode.is_dir=0;
    new_inode.size=0;
    inode_write(ino,&new_inode);
    int r = dir_add_entry(parent_ino,name,ino);
    if( r < 0 )
    {
        inode_free(ino);
        return r;
    }
    return ino;
    


}

/*
 * file_read — read `len` bytes from inode `ino` starting at `offset`
 * into `buf`. Reads stop at EOF if offset+len exceeds file size.
 * Returns: number of bytes actually read (>= 0), or a negative FS_ERR_*
 */
int file_read(int ino, void *buf, uint32_t len, uint32_t offset)
{
    if(ino<0 || ino>=FS_MAX_INODES || buf==NULL)
    return FS_ERR_BAD_ARG;

    Inode inode;
    if(inode_read(ino,&inode)!=FS_OK)
    return FS_ERR_IO;

    if(offset>inode.size) //cant read past end of file
    return 0;

    if(offset+len > inode.size) // adjust length such that it can be read until the end of file
    {
        len=inode.size - offset;
    }

    uint32_t bytes_read = 0;
    uint32_t current_offset = offset;

    while(bytes_read < len)
    {
        //logical block in the inode
        uint32_t ino_block_idx = current_offset/FS_BLOCK_SIZE; 
        //where in the block to start from
        uint32_t block_offset = current_offset % FS_BLOCK_SIZE; 

        int to_copy = FS_BLOCK_SIZE - block_offset; // Number of bytes to copy from the block

        if(to_copy > len - bytes_read)
        to_copy = len - bytes_read;

        int block_num = inode_get_block(ino,ino_block_idx);
        if(block_num<0)
        return block_num;
        uint8_t tmp [FS_BLOCK_SIZE];

       if(disk_read_block(block_num,tmp)!=FS_OK)
       return FS_ERR_IO;

       memcpy((uint8_t *)buf+bytes_read,tmp+block_offset,to_copy);

       bytes_read += to_copy;
       current_offset += to_copy;



    }

    return bytes_read;

}

/*
 * file_write — write `len` bytes from `buf` into inode `ino` starting
 * at `offset`. Extends the file if offset+len exceeds current size.
 * Returns: number of bytes actually written (>= 0), or a negative FS_ERR_*
 */
int file_write(int ino, const void *buf, uint32_t len, uint32_t offset)
{

    if(ino<0 || ino>=FS_MAX_INODES || buf==NULL)
    return FS_ERR_BAD_ARG;

    Inode inode;
    if(inode_read(ino,&inode)!=FS_OK)
    return FS_ERR_IO;


    uint32_t bytes_written = 0;
    uint32_t current_offset = offset;

    while(bytes_written < len)
    {
        uint32_t ino_block_idx = current_offset/FS_BLOCK_SIZE;
        uint32_t block_offset = current_offset % FS_BLOCK_SIZE;

        uint32_t to_write = FS_BLOCK_SIZE - block_offset;

        if(to_write > len - bytes_written)
            to_write = len-bytes_written;
        
        int block_num = inode_get_block(ino,ino_block_idx);
        if(block_num<0)
        return block_num;

        if(block_num<FS_DATA_START)
        block_num = inode_append_block(ino);
        if(block_num <0)
        {
            if(bytes_written>0)
            break;
            return block_num;
        }
        

        uint8_t tmp[FS_BLOCK_SIZE];
        disk_read_block(block_num,tmp);
        memcpy(tmp+block_offset,(uint8_t *)buf+bytes_written,to_write);
        disk_write_block(block_num,tmp);
        bytes_written += to_write;
        current_offset += to_write;

    }
    if(offset+bytes_written > inode.size)
    inode.size=offset+bytes_written;
    inode_write(ino,&inode);
    return bytes_written;
}

/*
 * file_delete — remove the file named `name` from directory `parent_ino`,
 * free all its data blocks, and free its inode.
 * Returns: FS_OK, FS_ERR_NOT_FOUND, FS_ERR_IS_DIR, FS_ERR_BAD_ARG
 */
int file_delete(const char *name, int parent_ino)
{
     if(parent_ino>=128 || parent_ino<0 || name==NULL)
    return FS_ERR_BAD_ARG;  
    
    Inode parent_inode;
    inode_read(parent_ino,&parent_inode);
    if(parent_inode.is_dir==0)
    return FS_ERR_NOT_DIR;

  int ino=dir_lookup(parent_ino,name);
  if(ino<0)
  return ino;

  Inode inode;
  inode_read(ino,&inode);
  if(inode.is_dir==1)
  return FS_ERR_IS_DIR;  

  int p = dir_remove_entry(parent_ino,name);
  if(p<0)
  return p;

    //freeing direct blocks
  for(int i=0;i<FS_DIRECT_BLOCKS;i++)
  {
    uint32_t block = inode.blocks[i];
    if(block!=0)
    bitmap_free(block);

  }

  //freeing the indirect block
  if(inode.indirect>0)
  {
    uint8_t buf[FS_BLOCK_SIZE];
    disk_read_block(inode.indirect,buf);
     uint32_t *pointers = (uint32_t *)buf;
    for(int i = 0; i < FS_BLOCK_SIZE/4; i++) 
    {
    if(pointers[i] != 0)
     bitmap_free(pointers[i]);
    }


    bitmap_free(inode.indirect);
  }  

  inode_free(ino);
  return FS_OK;



}
