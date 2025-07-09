#pragma once

#include <semaphore.h>

struct Semaphore
{
    sem_t sem;
    Semaphore(int count=0);
    ~Semaphore();
    bool wait(void);
    void post(void);
    int value(void);
};
