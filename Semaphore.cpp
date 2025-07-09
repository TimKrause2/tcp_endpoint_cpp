#include "Semaphore.h"
#include <stdio.h>

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
