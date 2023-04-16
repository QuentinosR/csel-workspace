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
    char* str = "Coucou !";
    char buffRead[10] = {};

    int fdWrite = open("/dev/my_char_dev_0", O_WRONLY);
    if(fdWrite == -1){
        printf("Impossible to open file\n");
        return 1;
    }
    write(fdWrite, str, strlen(str) + 1); //Write '\0'
    close(fdWrite);

    int fdRead = open("/dev/my_char_dev_0", O_RDWR);

    read(fdRead, buffRead, strlen(str) + 1); //Read '\0'
    printf("String read : \"%s\" \n", buffRead);
    close(fdRead);
    return 0;
}
