#include <stdio.h>
#include<string.h>
#include <stdbool.h>
#include<stdint.h>

typedef struct __attribute__((packed))
{
    char name[24];
    bool in_use;
    uint8_t padding[3];
    char content[484];
} Slot  ;
static_assert(sizeof(Slot)==512,"Size is not 512 bytes");

void disk_init()
{
 FILE *fp = fopen("disk.bin","rb");
    if (fp!=NULL)
    {
        fclose(fp);
    
    }
    else
    {
        FILE *fp=fopen("disk.bin","wb");
        Slot data[8] = {0};
        fwrite(data,sizeof(Slot),8,fp);
        fclose(fp);
    }
}
// int disk_read_slot(struct Slot *p,int index)
// {
//     int slot;
//     FILE *fp=fopen("disk.bin","wb");
//     fseek(fp,index*512,SEEK_SET);
//     fread(&slot,sizeof(Slot),1,fp);
//     fclose(fp);
//     return slot;
// }
// int disk_write_slot(int index,int slot)
// {
//     FILE *f = fopen("disk.bin", "r+b");
//     fseek(f, index * 512, SEEK_SET);
//     fwrite(&slot, sizeof(Slot), 1, f);
//     fclose(f);
// }
int main()
{
    disk_init();
    
    return 0;
}
// YOu will implement read, write here