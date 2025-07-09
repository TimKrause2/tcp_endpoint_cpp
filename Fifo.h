#include "Semaphore.h"
#include <list>
#include <memory>

struct Fifo
{
    std::list<std::shared_ptr<char[]>> data;
    Semaphore write_sem;
    Semaphore fifo_sem;
    int N_fifo;
    Fifo(int N_fifo);
    ~Fifo();
    void write(std::shared_ptr<char[]> p);
    std::shared_ptr<char[]> read(void);
    int ready(void);
    bool full(void);
};
