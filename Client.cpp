#include "Endpoint.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#define N_ENDPOINTS 4
#define N_THREADS 2

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
    printf("process_packet_data e=%p sp=%p\n", e, sp.get());
}

void client_recv_cb(Endpoint *e, std::shared_ptr<char[]> sp){
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

void send_data_packet(EndpointContainer *econt)
{
    char pdata[64];
    std::shared_ptr<char[]> sp = packet_data_new(pdata, 64, P_DATA_CODE_RAW_DATA);
    econt->sem.wait();
    if(econt->valid){
        econt->e->sendPacket(sp);
    }
    econt->sem.post();
}

int main(int argc, char **argv)
{
    EndpointContext ec(N_THREADS, N_ENDPOINTS, client_recv_cb);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    int sfd;
    int result;
    struct addrinfo hints;
    struct addrinfo *addrinfo_res;

    if( argc < 3 ){
        printf("Usage:\n%s <hostname> <service or port>\n", argv[0] );
        exit( 1 );
    }

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    printf("client2 main: calling getaddrinfo.\n");
    result = getaddrinfo( argv[1], argv[2], &hints, &addrinfo_res );
    if( result != 0 ){
        printf("hostname lookup failed: %s\n", gai_strerror( result ) );
        exit( 1 );
    }
    printf("client2 main: getaddrinfo returned. calling socket\n");
    sfd = socket( addrinfo_res->ai_family,
                  addrinfo_res->ai_socktype,
                  addrinfo_res->ai_protocol );
    if( sfd == -1 ){
        perror( "socket" );
        exit( 1 );
    }
    printf("client2 main: socket returned. calling connect.\n");
    result = connect( sfd, (const struct sockaddr*)addrinfo_res->ai_addr, addrinfo_res->ai_addrlen );
    if( result == -1 ){
        perror( "connect" );
        exit( 1 );
    }

    printf("connection established\n");

    EndpointContainer* econt =
            ec.newEndpoint(sfd, false);

    printf("entering main loop\n");

    while(active && econt->valid){
        send_data_packet(econt);
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000000/60;
        nanosleep(&ts, NULL);
    }
}
