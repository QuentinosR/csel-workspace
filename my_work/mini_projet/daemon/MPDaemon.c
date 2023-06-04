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

//Logs appear in /var/log/messages. Ex: Jan  1 00:13:23 csel local3.info csel_syslog[268]: [FREQ] 4.00
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
#include "ssd1306.h"
#include <limits.h>
#include "daemon.h"

#define GPIO_EXPORT   "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED      "/sys/class/gpio/gpio362"
#define GPIO_K1      "/sys/class/gpio/gpio0"
#define GPIO_K2      "/sys/class/gpio/gpio2"
#define GPIO_K3      "/sys/class/gpio/gpio3"
#define COOLING_CONTROLLER "/sys/class/mpcooling/controller"
#define K1            "0"
#define K2            "2"
#define K3            "3"
#define LED           "362"
#define NB_BUTTONS 3
#define NB_COOLING_CONTROLLER_ATTR 2

#define UNUSED(x) (void)(x)

//Common header ?
#define FIFO_PATH "/tmp/MPFifo"
static char coolingMode = 1;
static char blinkingFreq = 5;

//Return error code
static int open_buttons(int* fd, int size){
    char str_conc[255]; // Used to build path to files

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
        write(f, "rising", 6);
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
    write(f, "0", 1); //Turn off led
    
    return f;
}

static int open_pipe(){
    int fd;
    
    // Create the named pipe
    mkfifo(FIFO_PATH, 0666);

    fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    return 0;
}
static void action_buttons(int ibut, int fd_led, int fd_mode, int fd_blinking){
    char str[2 + 1];
    int fd = 0;
    int nb = 0;

    switch(ibut){
        case 0:
            printf("S1\n");
            if(blinkingFreq != CHAR_MAX)
                blinkingFreq += 1;
            fd = fd_blinking;
            nb = blinkingFreq;
            break;
        case 1:
            printf("S2\n");
            if(blinkingFreq !=0)
                blinkingFreq -= 1;
            fd = fd_blinking;
            nb = blinkingFreq;
            break;
        case 2:
            printf("S3\n");
            coolingMode  = !coolingMode;
            fd = fd_mode;
            nb = coolingMode;
            break;
        default:
            break;
    }
    pwrite(fd_led, "1", sizeof("1"), 0);
    usleep(2000);
    pwrite(fd_led, "0", sizeof("1"), 0);

    sprintf(str, "%d", nb);
    pwrite(fd, str, strlen(str), 0);
}

//[fd_mode, fd_blinking]
static int open_cooling_controller(int* fd){
    if(!fd)
        return -1;
    int fd_mode = open(COOLING_CONTROLLER "/mode", O_RDWR);
    if(fd_mode < 0)
        return -1;

    int fd_blinking = open(COOLING_CONTROLLER "/blinking", O_RDWR);
    if(fd_blinking < 0)
        return -1;
    
    fd[0] = fd_mode;
    fd[1] = fd_blinking;

    return 0;
}
int main(int argc, char* argv[])
{
    int ret;
    UNUSED(argc);
    UNUSED(argv);
/*
    ssd1306_init();

    ssd1306_set_position (0,0);
    ssd1306_puts("CSEL1a - SP.07");
    ssd1306_set_position (0,1);
    ssd1306_puts("  Demo - SW");
    ssd1306_set_position (0,2);
    ssd1306_puts("--------------");

    ssd1306_set_position (0,3);
    ssd1306_puts("Temp: 35'C");
    ssd1306_set_position (0,4);
    ssd1306_puts("Freq: 1Hz");
    ssd1306_set_position (0,5);
    ssd1306_puts("Duty: 50%");
    return 0;
*/

    openlog(NULL, LOG_NDELAY | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Before deamon");

    daemon_create();
    syslog(LOG_INFO, "After create !");

    int fd_cooling[NB_COOLING_CONTROLLER_ATTR]; 
    ret = open_cooling_controller(fd_cooling);
    if(ret < 0){
        printf("Error during opening controller fd\n");
        return 1;
    }
    int fd_mode = fd_cooling[0];
    int fd_blinking = fd_cooling[1];

    int fd_buttons[NB_BUTTONS]; 
    open_buttons(fd_buttons, NB_BUTTONS);

    int fd_led = open_led();

    int fd_pipe = open_pipe();

    int epfd = epoll_create1(0);
    if (epfd == -1){
        perror("Impossible to create poll group: ");
        return 1;
    }
    
    struct epoll_event events_buttons_conf[NB_BUTTONS];
    struct epoll_event event_pipe_conf;

    for(int i = 0; i < NB_BUTTONS; i++){
        events_buttons_conf[i].events = EPOLLIN | EPOLLET; // WAIT READ + LEVEL
        events_buttons_conf[i].data.fd = fd_buttons[i]; //Identify available source
        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_buttons[i], &events_buttons_conf[i]);
        if (ret == -1){
            perror("Impossible to add fd button to poll group: ");
            return 1;
        }
    }

/*
    syslog(LOG_INFO, "Before poll add!");
    event_pipe_conf.events = EPOLLIN;
    event_pipe_conf.data.fd = fd_pipe;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_pipe, &event_pipe_conf);
    if (ret == -1){
        syslog(LOG_INFO, "Error poll add ! %d", ret);
        perror("Impossible to add fd pipe to poll group: ");
        return 1;
    }
    syslog(LOG_INFO, "After poll add !");
*/

    int avoidFirstEvents = 0;
    while(1){
        struct epoll_event event_occured;
        int nr = epoll_wait(epfd, &event_occured, 1, -1);
        if (nr == -1){
            perror("Event error: ");
            return 1;
        }

/*
        if(event_occured.data.fd == fd_pipe){
            char buf[256] = {0};
            read(fd_pipe, buf, 256);
            printf("Reader: Message received: %s\n", buf);
            continue;
        }
*/
        int ibut = 0;
        for(ibut = 0; ibut < NB_BUTTONS - 1; ibut++){
            if(event_occured.data.fd == fd_buttons[ibut]){
                break;
            }
        }
        if(avoidFirstEvents < NB_BUTTONS)
            avoidFirstEvents++;
        else
            action_buttons(ibut, fd_led, fd_mode, fd_blinking);

    }
    return 0;
}
