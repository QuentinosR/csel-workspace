#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#define PHYS_ADDR_START_ID 0x01c14200
#define PHYS_ADDR_END_ID 0x01c14210
#define ID_SIZE PHYS_ADDR_END_ID - PHYS_ADDR_START_ID

int main(){
    int pageSize = getpagesize();
    off_t offsetAlign = PHYS_ADDR_START_ID % pageSize;
    int physAddrAlign = PHYS_ADDR_START_ID - offsetAlign;
    int fd = open("/dev/mem", O_RDWR);
    void* mapped = mmap ( NULL, pageSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, physAddrAlign);
    if(mapped == MAP_FAILED){
        printf("Mapping error :( \n");
        return 1;
    }
    printf("Mapped with success ! :) \n");

    uint64_t mappedPtr = (uint64_t) mapped; //Void* is 8 bytes.
    u_int64_t* mappedRegist =  (uint64_t*) (mappedPtr + offsetAlign); //We want to move of a specific nb of bytes
    uint64_t chipID1 = *mappedRegist;
    uint64_t chipID2 = *(mappedRegist + 1); // Move of 8 bytes
    printf("CHIP ID : %lx, %lx\n", chipID1, chipID2);

    munmap(mapped, pageSize);
    close(fd);

    return 0;
}