#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

int main(){
    fd_set set;
    FD_ZERO(&set);

    int fd = open("/dev/inter_cnt", O_RDONLY);
    if(fd == -1){
        printf("Impossible to open file\n");
        return 1;
    }
    FD_SET(fd, &set); //We add our fd for reading
    while(1){
        if(select(1000, &set, NULL, NULL, NULL) == -1){
            printf("Select error\n");
            close(fd);
            return 1;
        }else{
        }
    }
    //Never called
    close(fd);
    return 0;
}
