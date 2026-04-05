#include <stdio.h>
void fs_create(char * filename,char * data);
void fs_read(char *name);
void fs_delete(char *name);
void disk_init(void);

int main() {
    printf("MicroFS Task 1\n");
    disk_init();
    fs_create("File_1","This is File 1");
    fs_create("File_2","This is File 2");
    fs_read("File_1");
    fs_read("File_2");
    fs_delete("File_2");
    fs_read("File_2");

    return 0;



}