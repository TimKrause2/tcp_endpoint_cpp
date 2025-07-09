#include "Endpoint.h"
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

#define N_ENDPOINTS 2
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

int main(int argc, char **argv)
{
    EndpointContext ec(N_THREADS, N_ENDPOINTS, server_recv_cb);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    int sfd;
    int result;
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

