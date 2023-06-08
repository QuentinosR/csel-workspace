#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../daemon/peripheral.h"

int main() {
    int fd;
    char command[100];

    // Open the named pipe for reading
    fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    printf("Commands: \n");
    printf("- blinking=X\n");
    printf("- mode=X\n");
    printf("- stop\n");

    while(1){
        printf("[MPIPC]# ");
        fgets(command, sizeof(command), stdin);
        command[strlen(command)-1] = '\0'; // Remove newline character from the input
        if(strcmp(command, "stop") == 0)
            break;
        write(fd, command, strlen(command));
    }

    // Close the named pipe
    close(fd);

    return 0;
}
