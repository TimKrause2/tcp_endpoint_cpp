#include "RecvFifo.h"

RecvFifo::RecvFifo(int N_fifo)
    : N_fifo(N_fifo),
      write_sem(N_fifo),
      read_sem(0),
      fifo_sem(1)
{
}

RecvFifo::~RecvFifo()
{
}

void RecvFifo::write(Endpoint *e, std::shared_ptr<char[]> sp)
{
    write_sem.wait();
    fifo_sem.wait();
    data.emplace_front(e, sp);
    fifo_sem.post();
    read_sem.post();
}

bool RecvFifo::read(Endpoint *&e, std::shared_ptr<char[]> &sp)
{
    if(!read_sem.wait()) return false;
    fifo_sem.wait();
    RecvDetail rd = data.back();
    data.pop_back();
    fifo_sem.post();
    write_sem.post();
    e = rd.e;
    sp = rd.sp;
    return true;
}

int RecvFifo::ready(void)
{
    return data.size();
}

bool RecvFifo::full(void)
{
    return data.size() == N_fifo;
}
