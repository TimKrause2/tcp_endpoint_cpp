#include "Endpoint.h"
#include "Fifo.h"
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
    switch(packet_get_code(sp.get())){
    case P_DATA_CODE_RAW_DATA:
        e->context.broadcastPacket(sp, e);
        break;
    case P_DATA_CODE_NEW_CONN:
    case P_DATA_CODE_DEL_CONN:
        break;
    case P_DATA_CODE_TELEMETRY:
        // fill in the src index of the packet
        packet_data_telemetry_set_src(sp, e->container->index);

        // broadcast the data packet to all other endpoints
        e->context.broadcastPacket(sp, e);
        break;
    }
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

struct ServerDetail
{
    void *arg;
    unsigned short code;
    ServerDetail(){}
    ServerDetail(void *arg, unsigned short code)
        : arg(arg), code(code) {}
};

pthread_t server_thread;
Fifo<ServerDetail> server_fifo(5);
Semaphore delete_sem(0);
std::list<Endpoint*> clients;
struct ClientUserData
{
    std::list<Endpoint*>::iterator it;
    unsigned int index;
};

void* server_thread_routine(void *nu)
{
    printf("server_thread_routine\n");
    while(active){
        ServerDetail sd;
        if(server_fifo.read(sd, 250)){
            printf("server_thread_routine new code\n");
            if(sd.code==P_DATA_CODE_NEW_CONN){
                Endpoint *e = (Endpoint*)sd.arg;
                // broadcast new connection to client list
                // and echo client list connections
                std::shared_ptr<char[]> sp_new =
                        packet_data_new_conn(e->container->index);
                for(auto &ce : clients){
                    ce->container->sendPacket(sp_new, nullptr);
                    std::shared_ptr<char[]> sp_client =
                        packet_data_new_conn(ce->container->index);
                    e->container->sendPacket(sp_client, nullptr);
                }
                // add this endpoint to the client list
                clients.push_front(e);
                ClientUserData *udata = (ClientUserData*)e->user_data;
                udata->it = clients.begin();
                udata->index = e->container->index;
            }else if(sd.code==P_DATA_CODE_DEL_CONN){
                ClientUserData *udata = (ClientUserData*)sd.arg;
                // remove from the client list
                clients.erase(udata->it);
                // broadcast the del conn to the client list
                std::shared_ptr<char[]> sp =
                    packet_data_del_conn(udata->index);
                for(auto &ce : clients){
                    ce->container->sendPacket(sp, nullptr);
                }
                delete udata;
                delete_sem.post();
            }
        }
    }
    printf("server_thread_routine exiting\n");
    return NULL;
}

void server_new_cb(Endpoint *e)
{
    // allocate the client user data
    ClientUserData *udata = new ClientUserData;
    e->user_data = (void*)udata;
    ServerDetail sd((void*)e, P_DATA_CODE_NEW_CONN);
    server_fifo.write(sd);
}

void server_delete_cb(Endpoint *e)
{
    if(!active) return;
    ClientUserData* udata = (ClientUserData*)e->user_data;
    ServerDetail sd((void*)udata, P_DATA_CODE_DEL_CONN);
    server_fifo.write(sd);
    delete_sem.wait();
}

int main(int argc, char **argv)
{
    int result;

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

    if( argc < 3 ){
        printf("Usage:%s <address or +(ipv4 and ipv6)> <service or port>\n",argv[0]);
        exit( 1 );
    }

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags |= AI_PASSIVE;

    char *name;
    if(*argv[1]=='+'){
        name = NULL;
    }else{
        name = argv[1];
    }
    result = getaddrinfo( name, argv[2], &hints, &ai_res );
    if( result ){
        printf("getaddrinfo error:%s\n",gai_strerror( result ) );
        exit( 1 );
    }

    int epfd = epoll_create1(0);
    if(epfd==-1){
        perror("epoll_create1(0)");
        exit(1);
    }
    printf("epfd:%d\n", epfd);
    struct epoll_event *revents;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;

    EndpointContext ec(N_THREADS, N_ENDPOINTS, server_recv_cb);
    ec.setNewCB(server_new_cb);
    ec.setDeleteCB(server_delete_cb);

    result = pthread_create(&server_thread, NULL, server_thread_routine, NULL);
    if(result!=0){
        printf("pthread_create error:%s\n", strerror(result));
        exit(1);
    }

    for(ai_ptr = ai_res;ai_ptr;ai_ptr = ai_ptr->ai_next) {
        sfd = socket( ai_ptr->ai_family,
                      ai_ptr->ai_socktype | SOCK_NONBLOCK,
                      ai_ptr->ai_protocol );
        if( sfd == -1 ){
            perror( "socket" );
            continue;
        }
        printf("socket sfd:%d\n", sfd);
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
        int r = epoll_wait(epfd, &ev, 1, 100);
        if(r==-1){
            perror("Server epoll_wait");
        }else if(r==1){
            if(ev.events & EPOLLIN){
                ec.newEndpoint(ev.data.fd, true);
            }
        }
    }

    pthread_join(server_thread, NULL);
    printf("Server main exiting.\n");
}

