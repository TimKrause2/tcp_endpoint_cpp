#include "Fifo.h"

Fifo::Fifo(int N_fifo)
    : N_fifo(N_fifo),
      write_sem(N_fifo),
      fifo_sem(1)
{
}

Fifo::~Fifo()
{
}

void Fifo::write(std::shared_ptr<char[]> p)
{
    write_sem.wait();
    fifo_sem.wait();
    data.push_front(p);
    fifo_sem.post();
}

std::shared_ptr<char[]> Fifo::read(void)
{
    fifo_sem.wait();
    std::shared_ptr<char[]> p = data.back();
    data.pop_back();
    fifo_sem.post();
    write_sem.post();
    return p;
}

int Fifo::ready(void)
{
    return data.size();
}

bool Fifo::full(void)
{
    return data.size() == N_fifo;
}
