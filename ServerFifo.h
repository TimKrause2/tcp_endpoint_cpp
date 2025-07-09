#include "Semaphore.h"
#include <list>
#include <memory>

class Endpoint;

struct ServerDetail
{
    void *arg;
    unsigned short code;
    ServerDetail(void *arg, unsigned short code)
        : arg(arg), code(code) {}
};

struct ServerFifo
{
    std::list<ServerDetail> data;
    Semaphore write_sem;
    Semaphore read_sem;
    Semaphore fifo_sem;
    int N_fifo;
    ServerFifo(int N_fifo);
    ~ServerFifo();
    void write(void *arg, unsigned short code);
    bool read(void  *&arg, unsigned short &code);
    int ready(void);
    bool full(void);
};
