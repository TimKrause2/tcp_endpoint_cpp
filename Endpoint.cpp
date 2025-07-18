#include "Endpoint.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

bool EndpointContainer::processRecv(void)
{
    bool valid_r;
    sem.wait();
    if(valid){
        e->processRecv();
        if(e->recv_state == RECV_ERROR){
            deleteEndpoint_internal();
        }
    }
    valid_r = valid;
    sem.post();
    return valid_r;
}

bool EndpointContainer::processSend(void)
{
    bool valid_r;
    sem.wait();
    if(valid){
        e->processSend();
        if(e->send_state == SEND_ERROR){
            deleteEndpoint_internal();
        }
    }
    valid_r = valid;
    sem.post();
    return valid;
}

void EndpointContainer::deleteEndpoint_internal(void)
{
    valid = false;
    e.reset(nullptr);
    context->active_sem.wait();
}

void EndpointContainer::deleteEndpoint(void)
{
    sem.wait();
    if(valid){
        deleteEndpoint_internal();
    }
    sem.post();
}

void EndpointContainer::sendPacket(
        std::shared_ptr<char[]> sp,
        Endpoint* exclude)
{
    sem.wait();
    if(valid){
        //printf("EndpointContext::broadcastPacket e=%p\n", endpoints[i].e);
        if(e.get() != exclude){
            e->sendPacket(sp);
        }
    }
    sem.post();
}

bool EndpointContainer::newEndpoint(int fd)
{
    bool allocated = false;
    // lock the semaphore
    sem.wait();
    // check for available
    if(!valid){
        e.reset(new Endpoint(*context, this, fd));
        valid = true;
        allocated = true;
        context->active_sem.post();
    }
    // unlock the semaphore
    sem.post();
    return allocated;
}

EndpointContext::EndpointContext(
        int N_threads,
        int N_endpoints,
        void (*recv_cb)(Endpoint *e, std::shared_ptr<char[]> sp))
    : N_threads(N_threads),
      N_endpoints(N_endpoints),
      recv_cb(recv_cb),
      recv_fifo(32),
      new_cb(nullptr),
      delete_cb(nullptr),
      threads_enabled(true)
{
    // allocate the container array
    endpoints.reset(new EndpointContainer[N_endpoints]);
    for(int i=0;i<N_endpoints;i++){
        endpoints[i].index = i;
        endpoints[i].context = this;
    }

    // create the epoll file descriptor
    epoll_fd = epoll_create1(0);
    if(epoll_fd==-1){
        perror("EndpointContext::EndpointContext epoll_create");
        throw;
    }

    // initialize the thread id storage and start threads
    threads.reset(new pthread_t[N_threads]);
    for(int i=0;i<N_threads;i++){
        int r = pthread_create(&threads[i], NULL, thread_routine, (void*)this);
        if(r!=0){
            printf("EndpointContext::EndpointContext pthread_create:%s\n", strerror(r));
            throw;
        }
    }

    int r = pthread_create(&recv_thread, NULL, recv_routine, (void*)this);
    if(r!=0){
        printf("EndpointContext::EndpointContext recv_thread:%s\n", strerror(r));
        throw;
    }
}

EndpointContext::~EndpointContext()
{
    //printf("EndpointContext::~EndpointContext\n");
    // terminate all current connections
    for(int i=0;i<N_endpoints;i++){
        endpoints[i].deleteEndpoint();
    }

    //printf("EndpointContext::~EndpointContext disabling threads.\n");
    threads_enabled = false;

    pthread_join(recv_thread, NULL);
    for(int i=0;i<N_threads;i++){
        pthread_join(threads[i], NULL);
    }

    // close the epoll file descriptor
    close(epoll_fd);
}

void* EndpointContext::thread_routine(void *arg)
{
    EndpointContext* ec = (EndpointContext*)arg;

    while(ec->threads_enabled){
        struct epoll_event event;
        int Nevents = epoll_wait(ec->epoll_fd, &event, 1, 100);
        if(Nevents==-1){
            perror("EndpointContext::thread_routine epoll_wait");
        }else if(Nevents){
            //printf("EndpointContext::thread_routine events=%d\n", event.events);
            EndpointContainer *econt = (EndpointContainer*)event.data.ptr;
            // check for errors first
            if(event.events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)){
                // destroy the endpoint
                econt->deleteEndpoint();
                continue;
            }
            if(event.events & EPOLLIN){
                if(!econt->processRecv())
                    continue;
            }
            if(event.events & EPOLLOUT){
                econt->processSend();
            }
        }
    }
    //printf("EndpointContext::thread_routine exiting.\n");
    return NULL;
}

void* EndpointContext::recv_routine(void *arg)
{
    EndpointContext* ec = (EndpointContext*)arg;

    while(ec->threads_enabled){
        Endpoint *e;
        std::shared_ptr<char[]> sp;
        RecvDetail rd;
        if(ec->recv_fifo.read(rd, 250)){
            ec->recv_cb(rd.e, rd.sp);
        }
    }
    //printf("EndpointContext::recv_routine exiting.\n");
    return NULL;
}



void socket_set_nonblock(int fd, int nonblock)
{
    int flags = fcntl(fd, F_GETFL);
    if(nonblock){
        flags |= O_NONBLOCK;
    }else{
        flags &= ~O_NONBLOCK;
    }
    fcntl(fd, F_SETFL, flags);
}

void ignore_sigpipe(void)
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_restorer = NULL;

    int result = sigaction(SIGPIPE, &sa, NULL);
    if(result==-1)
    {
        perror("ignore_sigpipe: sigaction(SIGPIPE,...)");
    }
}

int EndpointContext::endpointAccept(int sfd)
{
    int cfd;
    socklen_t peer_addr_size;
    struct sockaddr_storage peer_addr;
    peer_addr_size = sizeof(peer_addr);
    cfd = accept(sfd,
                  (struct sockaddr*)&peer_addr,
                  &peer_addr_size);
    if(cfd==-1){
        perror("endpoint_accept: accept");
        return cfd;
    }

    printf("EndpointContext::endpointAccept sfd=%d cfd=%d\n",sfd,cfd);

    return cfd;
}

EndpointContainer* EndpointContext::newEndpoint(
        int fd,
        bool server)
{
    int cfd;
    if(server){
        cfd = endpointAccept(fd);
    }else{
        cfd = fd;
    }
    if(active_sem.value()==N_endpoints){
        close(cfd);
        return nullptr;
    }
    socket_set_nonblock(cfd, 1);
    // find an available container
    int i;
    for(i=0;i<N_endpoints;i++){
        if(endpoints[i].newEndpoint(cfd))
            break;
    }
    return &endpoints[i];
}

void EndpointContext::broadcastPacket(
        std::shared_ptr<char[]> sp,
        Endpoint *src)
{
    //printf("EndpointContext::broadcastPacket src=%p\n", src);
    for(int i=0;i<N_endpoints;i++){
        endpoints[i].sendPacket(sp, src);
    }
}

void EndpointContext::setNewCB(void (*new_cb0)(Endpoint *e))
{
    new_cb = new_cb0;
}

void EndpointContext::setDeleteCB(void (*delete_cb0)(Endpoint *e))
{
    delete_cb = delete_cb0;
}

Endpoint::Endpoint(EndpointContext &context,
                   EndpointContainer *container,
                   int fd)
    : context(context),
      container(container),
      cfd(fd),
      send_fifo(32),
      send_timer(send_timer_cb, (void*)container),
      recv_timer(recv_timer_cb, (void*)container)
{
    send_timer.set(CONFIRM_TIMEOUT_S);
    recv_timer.set(WATCHDOG_TIMEOUT_S);

    send_state = SEND_OPEN;
    recv_state = RECV_HEADER;

    ev_cfd_rw.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    ev_cfd_rw.data.ptr = (void*)container;

    int r = epoll_ctl(context.epoll_fd, EPOLL_CTL_ADD, cfd, &ev_cfd_rw);
    if(r==-1){
        perror("Endpoint::Endpoint epoll_ctl");
    }

    if(context.new_cb) context.new_cb(this);
}

Endpoint::~Endpoint()
{
    printf("Endpoint::~Endpoint\n");
    int r = epoll_ctl(context.epoll_fd, EPOLL_CTL_DEL, cfd, NULL);
    if(r==-1){
        perror("Endpoint::~Endpoint epoll_ctl");
    }

    close(cfd);

    if(context.delete_cb) context.delete_cb(this);
}

void Endpoint::send_timer_cb(union sigval arg)
{
    // data hasn't been sent for the timeout interval
    // send a CONFIRM status packet
    //printf("Endpoint::send_timer_cb\n");
    EndpointContainer *econt = (EndpointContainer*)arg.sival_ptr;
    char* p = packet_status_new(P_ST_CODE_CONFIRM);
    if(p){
        std::shared_ptr<char[]> sp(p);
        econt->sendPacket(sp, nullptr);
    }else{
        printf("Endpoint::send_timer_cb null packet\n");
    }
}

void Endpoint::recv_timer_cb(union sigval arg)
{
    // data hasn't been received for the timeout interval
    // close the connection
    printf("Endpoint::recv_timer_cb\n");
    EndpointContainer *econt = (EndpointContainer*)arg.sival_ptr;
    econt->deleteEndpoint();
}

void Endpoint::sendPacket(std::shared_ptr<char[]> sp)
{
    if(send_fifo.full())
        return;
    send_fifo.write(sp);
    //printf("Endpoint::sendPacket send_state=%d\n",send_state);
    if(send_state==SEND_OPEN){
        // initiate the transfer
        //printf("Endpoint::sendPacket initiating transmission.\n");
        prepareSend();
        // create an EPOLLOUT event to initiate the transfer
        epoll_ctl(context.epoll_fd, EPOLL_CTL_MOD, cfd, &ev_cfd_rw);
    }
}

void Endpoint::prepareSend(void)
{
    send_fifo.read(send_sp);
    send_buf = send_sp.get();
    send_bytes = packet_get_length(send_buf);
    send_state = SEND_READY;
}

void Endpoint::processSend(void)
{
    //printf("Endpoint::processSend\n");
    if(send_state==SEND_OPEN){
        if(send_fifo.ready()){
            prepareSend();
        }else{
            return;
        }
    }else if(send_state==SEND_ERROR)
    {
        return;
    }
    while(true){
        ssize_t r = send(cfd, send_buf, send_bytes, 0);
        if(r==-1){
            if(errno==EAGAIN || errno==EWOULDBLOCK){
                if(send_state==SEND_READY){
                    send_state = SEND_INPROGRESS;
                }
                return;
            }else{
                perror("Endpoint::processSend send");
                send_sp.reset();
                send_state = SEND_ERROR;
                return;

            }
        }else if(r == send_bytes){
            send_sp.reset();
            send_timer.set(CONFIRM_TIMEOUT_S);
            if(send_fifo.ready()){
                prepareSend();
                continue;
            }else{
                send_state = SEND_OPEN;
                return;
            }
        }else{
            //printf("Endpoint::processSend partial send r=%ld\n", r);
            send_buf += r;
            send_bytes -= r;
            send_timer.set(CONFIRM_TIMEOUT_S);
        }
    }
}

void Endpoint::processRecv(void)
{
    //printf("Endpoint::processRecv\n");
    ssize_t r;
    while(true){
        if(recv_state==RECV_HEADER){
            char header[SIZEOF_PACKET_COMMON];
            r = recv(cfd, &header, sizeof(header), MSG_PEEK);
            if(r==-1){
                if(errno==EAGAIN || errno==EWOULDBLOCK){
                    return;
                }
                perror("Endpoint::processRecv recv(header)");
                recv_state = RECV_ERROR;
                return;
            }else if(r==0){
                printf("Endpoint::processRecv RECV_HEADER connection lost\n");
                recv_state = RECV_ERROR;
                return;
            }else if(r==SIZEOF_PACKET_COMMON){
                if(!packet_ok(header)){
                    printf("Endpoint::processRecv malformed packet header.\n");
                    recv_state = RECV_ERROR;
                    return;
                }
                recv_bytes = packet_get_length(header);
                recv_sp.reset(new char[recv_bytes]);
                recv_buf = recv_sp.get();
                recv_state = RECV_INPROGRESS;
                continue;
            }else{
                return;
            }
        }else if(recv_state==RECV_INPROGRESS){
            r = recv(cfd, recv_buf, recv_bytes, 0);
            if(r==-1){
                if(errno==EAGAIN || errno==EWOULDBLOCK){
                    return;
                }else{
                    perror("Endpoint::processRecv RECV_INPROGRES recv");
                    recv_sp.reset();
                    recv_state = RECV_ERROR;
                    return;
                }
            }else if(r==recv_bytes){
                recv_state = RECV_HEADER;
                recv_timer.set(WATCHDOG_TIMEOUT_S);
                //printf("Endpoint::processRecv e=%p sp=%p use_count=%ld\n", this, recv_sp.get(), recv_sp.use_count());
                if(!context.recv_fifo.full()){
                    RecvDetail rd(this, recv_sp);
                    context.recv_fifo.write(rd);
                }
                continue;
            }else if(r==0){
                printf("Endpoint::processRecv RECV_INPROGRESS connection lost\n");
                recv_state = RECV_ERROR;
                recv_sp.reset();
                return;
            }else{
                //printf("Endpoint::processRecv partial read r=%ld\n", r);
                recv_buf += r;
                recv_bytes -= r;
                recv_timer.set(WATCHDOG_TIMEOUT_S);
            }
        }else if(recv_state==RECV_ERROR){
            return;
        }
    }
}

