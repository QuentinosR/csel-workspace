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

#define S_1M 1024 * 1024
// echo $$ > /sys/fs/cgroup/cpu/cg1/cgroup.procs : Ajoute son propre PID dans le CGroup.
//Les enfants sont également bridés même si PID différent.
//Cela crash lors de la 20 ème allocation. Cela montre que l'allocation alloue de l'espace supplémentaire à celui demandé.

int main(int argc, char* argv[])
{
    int i = 0;
    
    while(1){
        char * test = malloc(S_1M);
        for(int i = 0; i < S_1M; i++){
            test[i] = 0;
        }
        if(test == NULL){
            break;
        }
        i++;
        printf("Allocation %d time\n", i);
        
    }
    printf("End after %d allocs\n", i);
    return 0;
}
