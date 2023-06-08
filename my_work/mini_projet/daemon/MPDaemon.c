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

#include <sys/epoll.h>
#include "ssd1306.h"
#include "peripheral.h"

#define UNUSED(x) (void)(x)

//Voir les permissions des fichiers.
//redirect stdout from driver

static void action_disp_timer_time_up(int* fd){

    int fd_mode = fd[0];
    int fd_blinking = fd[1];
    int fd_temperature = fd[2];
    char buff_mode[ATTR_MAX_VAL_CHARS + 1] = {0};
    char buff_blinking[ATTR_MAX_VAL_CHARS + 1] = {0};
    char buff_temperature[ATTR_MAX_VAL_CHARS + 1] = {0};

    pread(fd_temperature, buff_temperature, ATTR_MAX_VAL_CHARS, 0);
    pread(fd_blinking, buff_blinking, ATTR_MAX_VAL_CHARS, 0);
    pread(fd_mode, buff_mode, ATTR_MAX_VAL_CHARS, 0);

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

static void action_pipe_message(char* command, int fd_mode, int fd_blinking){
    char cmd[100];
    char val[100];
    char* equalsSign = strchr(command, '=');
    if (equalsSign == NULL){
        syslog(LOG_ERR, "[MPDaemon] From IPC, char \"=\" not detected");
        return;
    }
    
    strncpy(cmd, command, equalsSign - command);
    cmd[equalsSign - command] = '\0';
    strcpy(val, equalsSign + 1);

    char valNum = strtol(val, NULL, 10);
    if(strncmp(cmd, ATTR_NAME_MODE, strlen(ATTR_NAME_MODE) ) == 0){
        peripheral_apply(fd_mode, valNum);
    }else if(strncmp(cmd, ATTR_NAME_BLINKING, strlen(ATTR_NAME_BLINKING)) == 0){
        peripheral_apply(fd_blinking, valNum);
    }else{
        return;
    }
}

static void action_buttons(int ibut, int fd_led, int fd_mode, int fd_blinking){
    int fd = 0;
    char buff[ATTR_MAX_VAL_CHARS + 1] = {0};
    char newVal = 0;

    switch(ibut){
        case 0:
            syslog(LOG_INFO, "[MPDaemon] S1 button pushed");
            fd = fd_blinking;
            break;
        case 1:
            syslog(LOG_INFO, "[MPDaemon] S2 button pushed");
            fd = fd_blinking;
            break;
        case 2:
            syslog(LOG_INFO, "[MPDaemon] S3 button pushed");
            fd = fd_mode;
            break;
        default:
            return;
    }

    /* Red led blinking */
    pwrite(fd_led, "1", sizeof("1"), 0);
    usleep(2000);
    pwrite(fd_led, "0", sizeof("1"), 0);

    /* Read value from peripheral*/
    pread(fd, buff, ATTR_MAX_VAL_CHARS, 0);
    newVal = strtol(buff, NULL, 10);

    switch(ibut){
        case 0:
            newVal += BLINKING_INC;
            break;
        case 1:
            newVal -= BLINKING_INC;
            break;
        case 2:
            newVal = !newVal;
            break;
        default:
            return;
    }
    peripheral_apply(fd, newVal);
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

/* Create daemon */
    daemon_create();

    openlog(NULL, LOG_NDELAY | LOG_PID, LOG_DAEMON);

/* Open peripherals */
    display_init();

    int fd_cooling[NB_COOLING_CONTROLLER_ATTR]; 
    ret = open_cooling_controller(fd_cooling);
    if(ret < 0){
        syslog(LOG_ERR, "[MPDaemon] Impossible to open driver controller");
        return 1;
    }
    int fd_mode = fd_cooling[0];
    int fd_blinking = fd_cooling[1];

    int fd_buttons[NB_BUTTONS]; 
    ret = open_buttons(fd_buttons, NB_BUTTONS);
    if(ret < 0){
        syslog(LOG_ERR, "[MPDaemon] Impossible to open buttons");
        return 1;
    }

    int fd_disp_timer = open_timer();
    if(fd_disp_timer < 0){
        syslog(LOG_ERR, "[MPDaemon] Impossible to open timer");
        return 1;
    }

    int fd_led = open_led();
    if(fd_led < 0){
        syslog(LOG_ERR, "[MPDaemon] Impossible to open timer");
        return 1;
    }

    int fd_pipe = open_pipe();
    if(fd_pipe < 0){
        syslog(LOG_ERR, "[MPDaemon] Impossible to open pipe");
        return 1;
    }

    int epfd = epoll_create1(0);
    if (epfd == -1){
        syslog(LOG_ERR, "[MPDaemon] Impossible to create poll group");
        return 1;
    }
    
    struct epoll_event events_buttons_conf[NB_BUTTONS];
    struct epoll_event event_pipe_conf;
    struct epoll_event event_disp_timer_conf;


/* Register epoll events */

    for(int i = 0; i < NB_BUTTONS; i++){
        events_buttons_conf[i].events = EPOLLIN | EPOLLET; // WAIT READ + LEVEL
        events_buttons_conf[i].data.fd = fd_buttons[i]; //Identify available source
        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_buttons[i], &events_buttons_conf[i]);
        if (ret == -1){
            syslog(LOG_ERR, "[MPDaemon] Impossible to add fd buttons to poll group");
            return 1;
        }
    }

    event_disp_timer_conf.events = EPOLLIN;
    event_disp_timer_conf.data.fd = fd_disp_timer;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_disp_timer, &event_disp_timer_conf);
    if (ret == -1){
        syslog(LOG_ERR, "[MPDaemon] Impossible to add fd timer to poll group");
        return 1;
    }


    event_pipe_conf.events = EPOLLIN;
    event_pipe_conf.data.fd = fd_pipe;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd_pipe, &event_pipe_conf);
    if (ret == -1){
        syslog(LOG_ERR, "[MPDaemon] Impossible to add fd pipe to poll group");
        return 1;
    }

/* Wait event and do corresponding action*/
    int avoidFirstEvents = 0;
    while(1){
        struct epoll_event event_occured;
        int nr = epoll_wait(epfd, &event_occured, 1, -1);
        if (nr == -1)
            continue;

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
                action_pipe_message(buf, fd_mode, fd_blinking);
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
