#include <stdio.h>
#include<string.h>
#include <stdbool.h>
#include<stdint.h>

typedef struct 
{
    char name[24];
    uint8_t in_use;
    uint8_t padding[3];
    char content[484];
} __attribute__((packed)) Slot  ;
static_assert(sizeof(Slot)==512,"Size is not 512 bytes");

void disk_init()
{
 FILE *fp = fopen("disk.bin","rb");
    if (fp!=NULL)
    {
        fclose(fp);
        return;
    }
    else
    {
        FILE *fp=fopen("disk.bin","wb");
        Slot data[8] = {0};
        fwrite(data,sizeof(Slot),8,fp);
        fclose(fp);
        return;
    }
}
void disk_read_slot(Slot *p,int index)
{
    
    FILE *fp=fopen("disk.bin","rb");
    fseek(fp,index*512,SEEK_SET);
    fread(p,sizeof(Slot),1,fp);
    fclose(fp);
    
}
int disk_write_slot(Slot *p,int index)
{
    FILE *f = fopen("disk.bin", "r+b");
    fseek(f, index * 512, SEEK_SET);
    fwrite(p, sizeof(Slot), 1, f);
    fclose(f);
}
int main()
{
    disk_init();
    Slot test1,test2;
    strncpy(test1.name,"struct",6);
    test1.in_use=1;

    disk_write_slot(&test1,2);
    disk_read_slot(&test2,2);
    printf("%s",test2.name);
    
    return 0;
}
// YOu will implement read, write here