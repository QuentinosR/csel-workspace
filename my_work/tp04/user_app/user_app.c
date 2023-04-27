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

/*
 * status led - gpioa.10 --> gpio10
 * power led  - gpiol.10 --> gpio362
 */
#define GPIO_EXPORT   "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED      "/sys/class/gpio/gpio10"
#define GPIO_K1      "/sys/class/gpio/gpio0"
#define GPIO_K2      "/sys/class/gpio/gpio2"
#define GPIO_K3      "/sys/class/gpio/gpio3"
#define K1            "0"
#define K2            "2"
#define K3            "3"
#define LED           "10"
#define NB_BUTTONS 3

//Return error code
static int open_buttons(int* fd, int size){
    char str_conc[255];

    for(int i = 0; i <= size -1; i++){
        char* gpio_path;
        char* gpio_num;

        switch(i){
            case 0:
                gpio_path = GPIO_K1;
                gpio_num = K1;
                break;
            case 1:
                gpio_path = GPIO_K2;
                gpio_num = K2;
                break;
            case 2:
                gpio_path = GPIO_K3;
                gpio_num = K3;
                break;
            default:
                break;
        }
        // unexport pin out of sysfs (reinitialization)
        int f = open(GPIO_UNEXPORT, O_WRONLY);
        write(f, gpio_num, strlen(gpio_num));
        close(f);

        // export pin to sysfs
        f = open(GPIO_EXPORT, O_WRONLY);
        write(f, gpio_num, strlen(gpio_num));
        close(f);

        // config pin
        sprintf(str_conc, "%s%s", gpio_path, "/direction");
        f = open(str_conc, O_WRONLY);
        write(f, "in", 2);
        close(f);

        // config interrupt
        sprintf(str_conc, "%s%s", gpio_path, "/edge");
        f = open(str_conc, O_WRONLY);
        write(f, "both", 5);
        close(f);

        // open gpio value attribute
        sprintf(str_conc, "%s%s", gpio_path, "/value");
        f = open(str_conc, O_RDWR);

        fd[i] = f;
    }
    
    return 1; // to be modifed to return error 
}
static int open_led()
{
    // unexport pin out of sysfs (reinitialization)
    int f = open(GPIO_UNEXPORT, O_WRONLY);
    write(f, LED, strlen(LED));
    close(f);

    // export pin to sysfs
    f = open(GPIO_EXPORT, O_WRONLY);
    write(f, LED, strlen(LED));
    close(f);

    // config pin
    f = open(GPIO_LED "/direction", O_WRONLY);
    write(f, "out", 3);
    close(f);

    // open gpio value attribute
    f = open(GPIO_LED "/value", O_RDWR);
    
    return f;
}

int main(int argc, char* argv[])
{
    /*
    long duty   = 2;     // %
    long period = 1000;  // ms
    if (argc >= 2) period = atoi(argv[1]);
    period *= 1000000;  // in ns

    // compute duty period...
    long p1 = period / 100 * duty;
    long p2 = period - p1;

    int led = open_led();
    pwrite(led, "1", sizeof("1"), 0);

    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    int k = 0;
    while (1) {
        struct timespec t2;
        clock_gettime(CLOCK_MONOTONIC, &t2);

        long delta =
            (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);

        int toggle = ((k == 0) && (delta >= p1)) | ((k == 1) && (delta >= p2));
        if (toggle) {
            t1 = t2;
            k  = (k + 1) % 2;
            if (k == 0)
                pwrite(led, "1", sizeof("1"), 0);
            else
                pwrite(led, "0", sizeof("0"), 0);
        }
    }
    */
    int fd_buttons[NB_BUTTONS]; 
    open_buttons(fd_buttons, NB_BUTTONS);

    int epfd = epoll_create1(0);
    if (epfd == -1)
        printf("impossible to create poll group\n");
    
    struct epoll_event events_conf[NB_BUTTONS];

    for(int i = 0; i < NB_BUTTONS; i++){
        events_conf[i].events = EPOLLIN | EPOLLET; // WAIT READ + LEVEL
        events_conf[i].data.fd = fd_buttons[i]; //Identify available source
        int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_buttons[i], &events_conf[i]);
        if (ret == -1)
            printf("impossible to add fd to poll group\n");
        }

    while(1){
        struct epoll_event event_occured;
        int nr = epoll_wait(epfd, &event_occured, NB_BUTTONS, -1);
        if (nr == -1)
            printf("event error\n");

        printf ("event=%ld on fd=%d\n", event_occured.events, event_occured.data.fd);
        int ibut = 0;
        for(ibut = 0; ibut < NB_BUTTONS - 1; ibut++){
            if(event_occured.data.fd == fd_buttons[ibut]){
                break;
            }
        }

        char but_val[2];
        ssize_t n = pread(fd_buttons[ibut],but_val, 1, 0);

        printf("button %d val: %s\n", ibut + 1, but_val);
        
       
    }
    return 0;
}
