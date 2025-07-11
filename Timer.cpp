#include "Timer.h"
#include <string.h>
#include <stdio.h>

Timer::Timer(void (*cb)(union sigval), void *arg)
{
    struct sigevent se;
    memset(&se, 0, sizeof(se));
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_notify_function = cb;
    se.sigev_value.sival_ptr = arg;

    int r = timer_create(CLOCK_MONOTONIC, &se, &tid);
    if(r==-1){
        perror("Timer::Timer timer_create");
        initialized = false;
    }else{
        initialized = true;
    }
}

Timer::~Timer(void)
{
    if(!initialized)return;
    int r = timer_delete(tid);
    if(r==-1){
        perror("Timer::~Timer timer_delete");
    }
}

void Timer::set(int seconds)
{
    if(!initialized)return;
    struct itimerspec ts;
    memset(&ts, 0, sizeof(ts));
    ts.it_value.tv_sec = seconds;

    int r = timer_settime(tid, 0, &ts, NULL);
    if(r==-1){
        perror("Timer::set timer_settime");
    }
}

void Timer::disarm(void)
{
    if(!initialized)return;
    struct itimerspec ts;
    memset(&ts, 0, sizeof(ts));

    int r = timer_settime(tid, 0, &ts, NULL);
    if(r==-1){
        perror("Timer::disarm timer_settime");
    }
}
