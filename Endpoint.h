#include "Protocol.h"
#include "Timer.h"
#include "Fifo.h"
#include "Semaphore.h"
#include "RecvFifo.h"
#include <map>
#include <memory>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

class Endpoint;
class EndpointContext;

struct EndpointContainer
{
    bool valid;
    Endpoint *e;
    Semaphore sem;
    EndpointContext *context;
    EndpointContainer():valid(false),e(nullptr),sem(1){}
};

class EndpointContext
{
    std::unique_ptr<EndpointContainer[]> endpoints;
    int N_threads;
    int N_endpoints;
    void (*recv_cb)(Endpoint *e, std::shared_ptr<char[]> sp);
    pthread_t recv_thread;

    std::unique_ptr<pthread_t[]> threads;
    static void* thread_routine(void *arg);
    static void* recv_routine(void *arg);
    int endpointAccept(int sfd);
public:
    RecvFifo recv_fifo;
    Semaphore active_sem;
    int epoll_fd;
    EndpointContext(
            int N_threads,
            int N_endpoints,
            void (*recv_cb)(Endpoint *e, std::shared_ptr<char[]> sp));
    ~EndpointContext();
    EndpointContainer* newEndpoint(
            int  fd,
            bool server);
    void deleteEndpoint();

    void broadcastPacket(std::shared_ptr<char[]> sp, Endpoint *src=nullptr);
};

enum {
    SEND_OPEN, // interface is available
    SEND_READY, // buffers have been initialized
    SEND_INPROGRESS, // packet transmission is in progress
    SEND_VERIFY, // packet has been transmitted and wainting for final event
    SEND_ERROR
};

enum {
    RECV_HEADER,
    RECV_INPROGRESS,
    RECV_DISCARD,
    RECV_ERROR
};

class Endpoint
{
public:
    EndpointContext &context;
    EndpointContainer *container;
    int cfd;
    Timer send_timer;
    Timer recv_timer;
    bool user_destroy;

    int    send_state;
    std::shared_ptr<char[]> send_sp;
    char  *send_buf;
    size_t send_bytes;
    Fifo   send_fifo;

    int recv_state;
    std::shared_ptr<char[]> recv_sp;
    char* recv_buf;
    size_t recv_bytes;

    struct epoll_event ev_cfd_rw;
public:
    Endpoint(
            EndpointContext &context,
            EndpointContainer *container,
            int  fd);
    ~Endpoint();
    static void send_timer_cb(union sigval);
    static void recv_timer_cb(union sigval);
    void sendPacket(std::shared_ptr<char[]> sp);
    void processSend(void);
    void processRecv(void);
};
