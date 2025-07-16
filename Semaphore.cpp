#include "Semaphore.h"
#include <stdio.h>
#include <time.h>
#include <errno.h>

Semaphore::Semaphore(int count)
{
    int r = sem_init(&sem, 0, count);
    if(r==-1){
        perror("Semaphore::Semaphore sem_init");
    }
}

Semaphore::~Semaphore()
{
    int r = sem_destroy(&sem);
    if(r==-1){
        perror("Semaphore::~Semaphore sem_destroy");
    }
}

bool Semaphore::wait(void)
{
    int r = sem_wait(&sem);
    if(r==-1){
        perror("Semaphore::wait sem_wait");
        return false;
    }else{
        return true;
    }
}

bool Semaphore::timedwait(int millisec)
{
    struct timespec ts_now;
    struct timespec ts_delay;
    struct timespec ts_abs;

    // initialize the delay
    ts_delay.tv_sec = millisec/1000;
    ts_delay.tv_nsec = (millisec%1000)*1000000;

    // get the time since epoch
    clock_gettime(CLOCK_REALTIME, &ts_now);

    // calculate the absolute timeout
    ts_abs.tv_sec = ts_now.tv_sec + ts_delay.tv_sec;
    ts_abs.tv_nsec = ts_now.tv_nsec + ts_delay.tv_nsec;
    if(ts_abs.tv_nsec >= 1000000000){
        ts_abs.tv_sec++;
        ts_abs.tv_nsec-=1000000000;
    }

    // proceed to wait for the semaphore
    int r = sem_timedwait(&sem, &ts_abs);
    if(r==-1){
        if(errno!=ETIMEDOUT){
            perror("Semaphore::timedwait sem_timedwait");
        }
        return false;
    }
    return true;
}

void Semaphore::post(void)
{
    int r = sem_post(&sem);
    if(r==-1){
        perror("Semaphore::post sem_post");
    }
}

int Semaphore::value(void)
{
    int val;
    int r = sem_getvalue(&sem, &val);
    if(r==-1){
        perror("Semaphore::value sem_getvalue");
        return 0;
    }
    return val;
}
