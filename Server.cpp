#include "Endpoint.h"
#include "ServerFifo.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define N_ENDPOINTS 4
#define N_THREADS 2
#define LISTEN_BACKLOG 2

void process_packet_status(Endpoint *e, std::shared_ptr<char[]> sp){
    switch(packet_get_code(sp.get())){
    case P_ST_CODE_READY:
        break;
    case P_ST_CODE_BUSY:
        break;
    case P_ST_CODE_CONFIRM:
        printf("Confirm packet status.\n");
        break;
    }
}

void process_packet_data(Endpoint *e, std::shared_ptr<char[]> sp)
{
    // fill in the src index of the packet
    packet_data_telemetry_set_src(sp, e->container->index);

    // broadcast the data packet to all other endpoints
    e->context.broadcastPacket(sp, e);
}

void server_recv_cb(Endpoint *e, std::shared_ptr<char[]> sp){
    switch(packet_get_type(sp.get())){
    case P_STATUS:
        process_packet_status(e, sp);
        break;
    case P_DATA:
        process_packet_data(e, sp);
        break;
    }
}

bool active = true;

void sig_handler(int sig)
{
    printf("sig_handler\n");
    active = false;
}

pthread_t server_thread;
ServerFifo server_fifo(5);
Semaphore delete_sem(0);
std::list<Endpoint*> clients;
struct ClientUserData
{
    std::list<Endpoint*>::iterator it;
    unsigned int index;
};

void* server_thread_routine(void *arg)
{
    printf("server_thread_routine\n");
    while(true){
        void *arg;
        unsigned short code;
        if(server_fifo.read(arg, code)){
            printf("server_thread_routine new code\n");
            if(code==P_DATA_CODE_NEW_CONN){
                Endpoint *e = (Endpoint*)arg;
                // broadcast new connection to client list
                // and echo client list connections
                std::shared_ptr<char[]> sp_new =
                        packet_data_new_conn(e->container->index);
                for(auto &ce : clients){
                    ce->container->sem.wait();
                    ce->sendPacket(sp_new);
                    ce->container->sem.post();
                    e->container->sem.wait();
                    std::shared_ptr<char[]> sp_client =
                        packet_data_new_conn(ce->container->index);
                    e->sendPacket(sp_client);
                    e->container->sem.post();
                }
                // add this endpoint to the client list
                clients.push_front(e);
                ClientUserData *udata = (ClientUserData*)e->user_data;
                udata->it = clients.begin();
                udata->index = e->container->index;
            }else if(code==P_DATA_CODE_DEL_CONN){
                ClientUserData *udata = (ClientUserData*)arg;
                // remove from the client list
                clients.erase(udata->it);
                // broadcast the del conn to the client list
                char data[64];
                std::shared_ptr<char[]> sp =
                    packet_data_del_conn(udata->index);
                for(auto &ce : clients){
                    ce->container->sem.wait();
                    if(ce->container->valid)
                        ce->sendPacket(sp);
                    ce->container->sem.post();
                }
                delete udata;
                delete_sem.post();
            }
        }else{
            return NULL;
        }
    }
    return NULL;
}

void server_new_cb(Endpoint *e)
{
    // allocate the client user data
    ClientUserData *udata = new ClientUserData;
    e->user_data = (void*)udata;
    server_fifo.write((void*)e, P_DATA_CODE_NEW_CONN);
}

void server_delete_cb(Endpoint *e)
{
    ClientUserData* udata = (ClientUserData*)e->user_data;
    server_fifo.write((void*)udata, P_DATA_CODE_DEL_CONN);
    delete_sem.wait();
}

int main(int argc, char **argv)
{
    int result;

    EndpointContext ec(N_THREADS, N_ENDPOINTS, server_recv_cb);
    ec.setNewCB(server_new_cb);
    ec.setDeleteCB(server_delete_cb);

    result = pthread_create(&server_thread, NULL, server_thread_routine, NULL);
    if(result!=0){
        printf("pthread_create error:%s\n", strerror(result));
        exit(1);
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    int sfd;
    struct addrinfo hints;
    struct addrinfo *ai_res;
    struct addrinfo *ai_ptr;
    int N_l_sockets=0;
    struct sigaction sigact;

    if( argc < 2 ){
        printf("Usage:%s <service or port>\n",argv[0]);
        exit( 1 );
    }

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags |= AI_PASSIVE;

    result = getaddrinfo( NULL, argv[1], &hints, &ai_res );
    if( result ){
        printf("getaddrinfo error:%s\n",gai_strerror( result ) );
        exit( 1 );
    }

    int epfd = epoll_create1(0);
    if(epfd==-1){
        perror("epoll_create1(0)");
        exit(1);
    }
    struct epoll_event *revents;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;

    for(ai_ptr = ai_res;ai_ptr;ai_ptr = ai_ptr->ai_next) {
        sfd = socket( ai_ptr->ai_family,
                      ai_ptr->ai_socktype | SOCK_NONBLOCK,
                      ai_ptr->ai_protocol );
        if( sfd == -1 ){
            perror( "socket" );
            continue;
        }

        int reuseport=1;
        setsockopt(sfd,SOL_SOCKET,SO_REUSEPORT,&reuseport,sizeof(int));

        result = bind( sfd, ai_ptr->ai_addr, ai_ptr->ai_addrlen );
        if( result == -1 ){
            perror( "bind" );
            close(sfd);
            continue;
        }

        result = listen( sfd, LISTEN_BACKLOG );
        if( result == -1 ){
            perror( "listen" );
            close(sfd);
            continue;
        }

        ev.data.fd = sfd;
        result = epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);
        if(result==-1){
            perror("epoll_ctl");
            close(sfd);
            continue;
        }

        N_l_sockets++;
    }

    while(active){
        struct epoll_event ev;
        int r = epoll_wait(epfd, &ev, 1, -1);
        if(r==-1){
            perror("Server epoll_wait");
            break;
        }else if(r==1){
            if(ev.events & EPOLLIN){
                ec.newEndpoint(ev.data.fd, true);
            }
        }
    }
}

