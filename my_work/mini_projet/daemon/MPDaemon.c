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
#define NB_COOLING_CONTROLLER_ATTR 3
#define ATTR_MAX_VAL_CHARS 2
#define TIMER_1S_IN_NS 1000000000
#define DISPLAY_TIMER_START_INTERVAL_NS (TIMER_1S_IN_NS / 4)

#define UNUSED(x) (void)(x)

//Define in MPDaemon.h
#define FIFO_PATH "/tmp/MPFifo"
//Get initial values or define in MPDriver.h
static char coolingMode = 1;
static char blinkingFreq = 5;
//define attr strings
//concurrency problem with buttons and IPC but don't protect

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

static int open_timer(){
    struct itimerspec timer_conf;
	int fd_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd_timer == -1) {
		perror("Error in timer file creation: ");
	}

    int nbSeconds = DISPLAY_TIMER_START_INTERVAL_NS / TIMER_1S_IN_NS;
    int nbNanoSeconds = DISPLAY_TIMER_START_INTERVAL_NS % TIMER_1S_IN_NS;
	timer_conf.it_interval.tv_sec = nbSeconds;
	timer_conf.it_interval.tv_nsec = nbNanoSeconds;
	timer_conf.it_value.tv_sec = nbSeconds; //By default 2s
    timer_conf.it_value.tv_nsec = nbNanoSeconds;

    //long newIntervalNS = timer_conf.it_interval.tv_nsec + TIMER_1S_IN_NS * timer_conf.it_interval.tv_sec;
    
	if (timerfd_settime(fd_timer, 0, &timer_conf, NULL) < 0) {
		perror("Error during setting time: ");
	}

    return fd_timer;
}

static void action_disp_timer_time_up(int* fd){

    int fd_mode = fd[0];
    int fd_blinking = fd[1];
    int fd_temperature = fd[2];
    char buff_mode[ATTR_MAX_VAL_CHARS + 1] = {0};
    char buff_blinking[ATTR_MAX_VAL_CHARS + 1] = {0};
    char buff_temperature[ATTR_MAX_VAL_CHARS + 1] = {0};

    pread(fd_temperature, buff_temperature, 2, 0);
    pread(fd_blinking, buff_blinking, 2, 0);
    pread(fd_mode, buff_mode, 2, 0);

    ssd1306_set_position(6,3);
    ssd1306_puts(buff_mode);

    ssd1306_set_position(6,4);
    ssd1306_puts("  ");
    ssd1306_set_position(6,4);
    ssd1306_puts(buff_temperature);

    ssd1306_set_position(6,5);
    ssd1306_puts("  ");
    ssd1306_set_position(6,5);
    ssd1306_puts(buff_blinking);
    
}

static int open_pipe(){
    int fd;
    
    // Create the named pipe
    mkfifo(FIFO_PATH, 0666);

    fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open");
        syslog(LOG_ERR, "Error in opening pipe");
        exit(1);
    }
    return fd;
}

static void action_pipe_message(char* command){
    char cmd[100];
    char val[100];
    char* equalsSign = strchr(command, '=');
    if (equalsSign == NULL){
        syslog(LOG_ERR, "Char \"=\" not detected\n");
        return;
    }
    
    strncpy(cmd, command, equalsSign - command);
    cmd[equalsSign - command] = '\0';
    strcpy(val, equalsSign + 1);
    syslog(LOG_INFO, "Reader: Message received: cmd :%s, val:%s\n", cmd, val);
/*
    char valNum = strol(val, NULL, 10);
    if(strncmp(cmd, "mode", 4 ) == 0){
        //coolingMode  =
    }else if(strncmp(cmd, "blinking", 8) == 0){
   
    }else if(strncmp(cmd, "temperature", 4) == 0){
        
    }
*/
}
static void action_buttons(int ibut, int fd_led, int fd_mode, int fd_blinking){
    char str[2 + 1];
    int fd = 0;
    char nb = 0;
    char* pChange = NULL;

    switch(ibut){
        case 0:
            printf("[MPDaemon] S1 pushed\n");
            fd = fd_blinking;
            nb = blinkingFreq + 1;
            pChange = &blinkingFreq;
            break;
        case 1:
            printf("S2\n");
            fd = fd_blinking;
            nb = blinkingFreq - 1;
            pChange = &blinkingFreq;
            break;
        case 2:
            printf("S3\n");
            fd = fd_mode;
            nb = !coolingMode;
            pChange = &coolingMode;
            break;
        default:
            break;
    }

    /* Red led blinking */
    pwrite(fd_led, "1", sizeof("1"), 0);
    usleep(2000);
    pwrite(fd_led, "0", sizeof("1"), 0);

    sprintf(str, "%d", nb);

    /* If no error, modify the value */
    if(pwrite(fd, str, strlen(str), 0) > 0){
        *pChange = nb;
    }
}

//[fd_mode, fd_blinking, fd_temperature]
static int open_cooling_controller(int* fd){
    if(!fd)
        return -1;
    int fd_mode = open(COOLING_CONTROLLER "/mode", O_RDWR);
    if(fd_mode < 0)
        return -1;

    int fd_blinking = open(COOLING_CONTROLLER "/blinking", O_RDWR);
    if(fd_blinking < 0)
        return -1;

    int fd_temperature = open(COOLING_CONTROLLER "/temperature", O_RDONLY);
    if(fd_temperature < 0)
        return -1;
    
    fd[0] = fd_mode;
    fd[1] = fd_blinking;
    fd[2] = fd_temperature;
    return 0;
}
void display_init(){
    ssd1306_init();
    ssd1306_clear_display();
    ssd1306_set_position (0,0);
    ssd1306_puts("CSEL1 - MPCooler");
    ssd1306_set_position (0,1);
    ssd1306_puts("  Rod Quentin");
    ssd1306_set_position (0,2);
    ssd1306_puts("-----------------");

    ssd1306_set_position (0,3);
    ssd1306_puts("Mode:   ");
    ssd1306_set_position (0,4);
    ssd1306_puts("Temp:   'C");
    ssd1306_set_position (0,5);
    ssd1306_puts("Freq:   Hz");
}

int main(int argc, char* argv[])
{
    int ret;
    UNUSED(argc);
    UNUSED(argv);

    daemon_create();

    openlog(NULL, LOG_NDELAY | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "After create daemon !");
    display_init();


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

    int fd_disp_timer = open_timer();

    int fd_led = open_led();

    int fd_pipe = open_pipe();

    int epfd = epoll_create1(0);
    if (epfd == -1){
        perror("Impossible to create poll group: ");
        return 1;
    }
    
    struct epoll_event events_buttons_conf[NB_BUTTONS];
    struct epoll_event event_pipe_conf;
    struct epoll_event event_disp_timer_conf;

    for(int i = 0; i < NB_BUTTONS; i++){
        events_buttons_conf[i].events = EPOLLIN | EPOLLET; // WAIT READ + LEVEL
        events_buttons_conf[i].data.fd = fd_buttons[i]; //Identify available source
        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_buttons[i], &events_buttons_conf[i]);
        if (ret == -1){
            perror("Impossible to add fd button to poll group: ");
            return 1;
        }
    }

    event_disp_timer_conf.events = EPOLLIN;
    event_disp_timer_conf.data.fd = fd_disp_timer;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_disp_timer, &event_disp_timer_conf);
    if (ret == -1){
        perror("Impossible to add fd timer to poll group: ");
        syslog(LOG_INFO, "Impossible to add fd timer to poll group");
        return 1;
    }


    event_pipe_conf.events = EPOLLIN;
    event_pipe_conf.data.fd = fd_pipe;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_pipe, &event_pipe_conf);
    if (ret == -1){
        syslog(LOG_INFO, "Error poll add ! %d, fd: %d", ret, fd_pipe);
        syslog(LOG_INFO, strerror(errno));
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

        if(event_occured.data.fd == fd_disp_timer){
            uint64_t value;
            read(fd_disp_timer, &value, 8);
            action_disp_timer_time_up(fd_cooling);
            continue;
        }

        if(event_occured.data.fd == fd_pipe){
            char buf[256] = {0};
            int ret = read(fd_pipe, buf, 256);
            if(ret != -1 && ret != 0){
                action_pipe_message(buf);
            }
            continue;
        }

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
