#define _GNU_SOURCE
#include <sched.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>

int stop;
void child_action(){
    int i = 0;
    while(!stop){
        i++;
    }
    printf("child finished\n");
    exit(0);
}

void parent_action(){
    int i = 0;
    int status;
    while(!stop){
        i++;
    }
    wait(&status);
    printf("parent finished\n");
    exit(0);
}

void sig_catch(int signum){
    stop = 1;
    return;
}
int main(int argc, char* argv[])
{
    signal(SIGINT, sig_catch);
    pid_t pid = fork();
    if (pid == 0) { // child
        child_action();
        
    } else { // parent
        parent_action();
    }
    return 0;
}
