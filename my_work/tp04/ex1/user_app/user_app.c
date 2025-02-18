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

#define TIMER_FREQUENCY 2
#define TIMER_1S_IN_NS 1000000000
#define TIMER_START_INTERVAL_NS (TIMER_1S_IN_NS / 2)

//Use syslog
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
    
    return f;
}

static int open_timer(){
    struct itimerspec timer_conf;
	int fd_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd_timer == -1) {
		perror("Error in timer file creation: ");
	}

    int nbSeconds = TIMER_START_INTERVAL_NS / TIMER_1S_IN_NS;
    int nbNanoSeconds = TIMER_START_INTERVAL_NS % TIMER_1S_IN_NS;
	timer_conf.it_interval.tv_sec = nbSeconds;
	timer_conf.it_interval.tv_nsec = nbNanoSeconds;
	timer_conf.it_value.tv_sec = nbSeconds; //By default 2s
    timer_conf.it_value.tv_nsec = nbNanoSeconds;

    long newIntervalNS = timer_conf.it_interval.tv_nsec + TIMER_1S_IN_NS * timer_conf.it_interval.tv_sec;
    printf("[FREQ] %.2f\n", TIMER_1S_IN_NS / (double)newIntervalNS);

	if (timerfd_settime(fd_timer, 0, &timer_conf, NULL) < 0) {
		perror("Error during setting time: ");
	}

    return fd_timer;
}

static void action_timer_time_up(int fd_led){
    static int k = 0;
    k = (k+1) % 2;
    if(k){
        pwrite(fd_led, "1", sizeof("1"), 0);
    }else{
        pwrite(fd_led, "0", sizeof("0"), 0);
    }
}

static void action_buttons(int ibut, int fd_timer){
    struct itimerspec timer_conf;
    if (timerfd_gettime(fd_timer, &timer_conf) < 0) {
       perror("Error during getting time: ");
       return;
    }

    long newIntervalNS = 0;
    switch(ibut){
        case 0:
            newIntervalNS = timer_conf.it_interval.tv_nsec + TIMER_1S_IN_NS * timer_conf.it_interval.tv_sec;
            newIntervalNS /= 2;
            break;
        case 2:
            newIntervalNS = timer_conf.it_interval.tv_nsec + TIMER_1S_IN_NS * timer_conf.it_interval.tv_sec;
            newIntervalNS *= 2;
            break;
        default:
            newIntervalNS = TIMER_START_INTERVAL_NS;
            break;
    }

    int nbSeconds = newIntervalNS / TIMER_1S_IN_NS;
    int nbNanoSeconds = newIntervalNS % TIMER_1S_IN_NS;

    timer_conf.it_interval.tv_nsec = nbNanoSeconds;
    timer_conf.it_interval.tv_sec = nbSeconds;

    printf("[FREQ] %.2f\n", TIMER_1S_IN_NS / (double)newIntervalNS);
    syslog(LOG_INFO, "[FREQ] %.2f\n", TIMER_1S_IN_NS / (double)newIntervalNS);
    if (timerfd_settime(fd_timer, 0, &timer_conf, NULL) < 0) {
        perror("Error during setting time: ");
    }
}
int main(int argc, char* argv[])
{
    openlog("csel_syslog", LOG_PID, LOG_LOCAL3); //Void function

    int fd_buttons[NB_BUTTONS]; 
    open_buttons(fd_buttons, NB_BUTTONS);

    int fd_timer = open_timer();
    int fd_led = open_led();

    int epfd = epoll_create1(0);
    if (epfd == -1){
        perror("Impossible to create poll group: ");
        return 1;
    }
    
    struct epoll_event events_buttons_conf[NB_BUTTONS];
    struct epoll_event event_timer_conf;

    for(int i = 0; i < NB_BUTTONS; i++){
        events_buttons_conf[i].events = EPOLLIN | EPOLLET; // WAIT READ + LEVEL
        events_buttons_conf[i].data.fd = fd_buttons[i]; //Identify available source
        int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_buttons[i], &events_buttons_conf[i]);
        if (ret == -1){
            perror("Impossible to add fd button to poll group: ");
            return 1;
        }
    }

    event_timer_conf.events = EPOLLIN;
    event_timer_conf.data.fd = fd_timer;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_timer, &event_timer_conf);
    if (ret == -1){
        perror("Impossible to add fd timer to poll group: ");
        return 1;
    }

    int avoidFirstEvents = 0;
    while(1){
        struct epoll_event event_occured;
        int nr = epoll_wait(epfd, &event_occured, 1, -1);
        if (nr == -1){
            perror("Event error: ");
            return 1;
        }

        if(event_occured.data.fd == fd_timer){
            uint64_t value;
            read(fd_timer, &value, 8);
            action_timer_time_up(fd_led);
        }else{
            int ibut = 0;
            for(ibut = 0; ibut < NB_BUTTONS - 1; ibut++){
                if(event_occured.data.fd == fd_buttons[ibut]){
                    break;
                }
            }
            if(avoidFirstEvents < NB_BUTTONS)
                avoidFirstEvents++;
            else
                action_buttons(ibut, fd_timer);
        }
    }
    return 0;
}
