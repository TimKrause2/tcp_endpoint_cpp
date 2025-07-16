#pragma once
#include "Semaphore.h"
#include <list>
#include <memory>

template <class T>
class Fifo
{
    std::list<T> data;
    int N_fifo;
    Semaphore write_sem;
    Semaphore read_sem;
    Semaphore fifo_sem;
public:
    Fifo(int N_fifo)
        : N_fifo(N_fifo),
          write_sem(N_fifo),
          read_sem(0),
          fifo_sem(1)
    {}
    ~Fifo(){}
    bool write(T obj, int timeout=-1)
    {
        if(timeout==-1){
            if(!write_sem.wait()) return false;
        }else{
            if(!write_sem.timedwait(timeout)) return false;
        }
        fifo_sem.wait();
        data.push_front(obj);
        fifo_sem.post();
        read_sem.post();
        return true;
    }
    bool read(T& obj, int timeout=-1)
    {
        if(timeout==-1){
            if(!read_sem.wait()) return false;
        }else{
            if(!read_sem.timedwait(timeout)) return false;
        }
        fifo_sem.wait();
        obj = data.back();
        data.pop_back();
        fifo_sem.post();
        write_sem.post();
        return true;
    }
    int ready(void)
    {
        return data.size();
    }
    bool full(void)
    {
        return data.size() == N_fifo;
    }
};
