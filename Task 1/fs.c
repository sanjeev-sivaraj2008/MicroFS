#include <stdio.h>
#include<string.h>
#include<stdint.h>

typedef struct 
{
    char name[24];
    uint8_t in_use;
    uint8_t padding[3];
    char content[484];
} __attribute__((packed)) Slot  ;
static_assert(sizeof(Slot)==512,"Size is not 512 bytes");

void disk_init(void);
void disk_read_slot(Slot *s,int index);
void disk_write_slot(Slot *s, int index);

void fs_create(char * filename,char * data)
{
    
for(int i=0;i<8;i++)
{
    Slot p;
    disk_read_slot(&p,i);
    if(p.in_use==0)
    {
        strncpy(p.name,filename,23);
        p.name[23]='\0';
        strncpy(p.content,data,483);
        p.content[483]='\0';
        p.in_use=1;
        disk_write_slot(&p,i);
        return;
    }
}
printf("All slots are used\n");
}
void fs_read(char *name)
{
    
    for(int i=0;i<8;i++)
    {
        Slot p;
        disk_read_slot(&p,i);
        if(p.in_use==1)
        {
        if(strcmp(p.name,name)==0)
        {
            printf("%s\n",p.content);
            
            return;
        }
    }
    }
    printf("Slot %s not found\n",name);


}

void fs_delete(char *name)
{
    for(int i=0;i<8;i++)
    {
        Slot p;
        disk_read_slot(&p,i);
        if(p.in_use==1)
        {
        if(strcmp(p.name,name)==0)
        {
            memset(&p,0,512);
            disk_write_slot(&p,i);
            return;

        }
    }
    }
    printf("Slot %s not found\n",name);
}

void main()
{
disk_init();


fs_create("hi","this is the hi file");
fs_create("bye","This is the bye file");
fs_read("hi");
fs_read("bye");
fs_delete("bye");
fs_read("bye");
}
//you will implement file system logic here