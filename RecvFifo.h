#include "Semaphore.h"
#include <list>
#include <memory>

class Endpoint;

struct RecvDetail
{
    Endpoint *e;
    std::shared_ptr<char[]> sp;
    RecvDetail(Endpoint *e, std::shared_ptr<char[]> sp)
        : e(e), sp(sp) {}
};

struct RecvFifo
{
    std::list<RecvDetail> data;
    Semaphore write_sem;
    Semaphore read_sem;
    Semaphore fifo_sem;
    int N_fifo;
    RecvFifo(int N_fifo);
    ~RecvFifo();
    void write(Endpoint *e,
               std::shared_ptr<char[]> sp);
    bool read(Endpoint *&e,
              std::shared_ptr<char[]> &sp,
              int timeout=-1);
    int ready(void);
    bool full(void);
};
