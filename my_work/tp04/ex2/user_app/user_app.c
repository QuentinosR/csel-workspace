/**
 * Copyright 2018 University of Applied Sciences Western Switzerland / Fribourg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Project: HEIA-FR / HES-SO MSE - MA-CSEL1 Laboratory
 *
 * Abstract: System programming -  file system
 *
 * Purpose: NanoPi silly status led control system
 *
 * Autĥor:  Daniel Gachet
 * Date:    07.11.2018
 */

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

int fd[2];

void child_action(){
    cpu_set_t set;
    char msgsWrite[5][10] = {"Coucou", "hey", "youhou", "hello", "exit"};
    int NB_MSG = 5;

    CPU_ZERO(&set);
    CPU_SET(1, &set);
    int ret = sched_setaffinity(0, sizeof(set), &set);
    if(ret == -1)
        exit(1);
    close(fd[0]); // close unused read descriptor

    for(int i = 0; i < NB_MSG; i++){
        int len = write(fd[1], msgsWrite[i], sizeof(msgsWrite[i]));
        if(len < 0)
            exit(1);
        sleep(1);
    }
    exit(0);
}

void parent_action(){
    cpu_set_t set;
    int status;
    char msgRead[10];

    CPU_ZERO(&set);
    CPU_SET(0, &set);
    int ret = sched_setaffinity(0, sizeof(set), &set);
    if(ret == -1)
        exit(1);

    close(fd[1]); // close unused write descriptor

    do{
        int len = read (fd[0], msgRead, sizeof(msgRead));
        if(len < 0)
            exit(1);
        printf("received text : \"%s\"\n", msgRead);
    }while(strcmp(msgRead, "exit") != 0);

    wait(&status);
    printf("finished\n");
    exit(0);
}

void sig_catch(int signum){
    return;
}
int main(int argc, char* argv[])
{
    //Signals handling are inherited with fork. 
    for (int sig = 1; sig < NSIG; sig++) {
        if (sig == SIGKILL || sig == SIGSTOP) {
            // Certains signaux ne peuvent pas être capturés
            continue;
        }
        signal(sig, sig_catch);
    }

    int err = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (err == -1){
        /* error*/
        printf("error\n");
        return 1;
    }
    pid_t pid = fork();
    if (pid == 0) { // child
        child_action();
        
    } else { // parent
        parent_action();
    }
    return 0;
}
