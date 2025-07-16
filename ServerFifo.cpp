#include "ServerFifo.h"

ServerFifo::ServerFifo(int N_fifo)
    : N_fifo(N_fifo),
      write_sem(N_fifo),
      read_sem(0),
      fifo_sem(1)
{
}

ServerFifo::~ServerFifo()
{
}

void ServerFifo::write(void *arg, unsigned short code)
{
    write_sem.wait();
    fifo_sem.wait();
    data.emplace_front(arg, code);
    fifo_sem.post();
    read_sem.post();
}

bool ServerFifo::read(
        void *&arg,
        unsigned short &code,
        int timeout)
{
    if(timeout==-1){
        if(!read_sem.wait()) return false;
    }else{
        if(!read_sem.timedwait(timeout)) return false;
    }
    fifo_sem.wait();
    ServerDetail sd = data.back();
    data.pop_back();
    fifo_sem.post();
    write_sem.post();
    arg = sd.arg;
    code = sd.code;
    return true;
}

int ServerFifo::ready(void)
{
    return data.size();
}

bool ServerFifo::full(void)
{
    return data.size() == N_fifo;
}
