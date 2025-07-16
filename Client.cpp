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

#define N_CLIENT_ENDPOINTS 4
#define N_ENDPOINTS 1
#define N_THREADS 1

struct Client
{
    bool valid;
    Telemetry t;
};

Client clients[N_CLIENT_ENDPOINTS];

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
    //printf("process_packet_data e=%p sp=%p\n", e, sp.get());
    char *pd = sp.get();
    unsigned int src_index;
    Telemetry *t;
    switch(packet_get_code(pd)){
    case P_DATA_CODE_RAW_DATA:
        printf("process_packet_data RAW_DATA\n");
        break;
    case P_DATA_CODE_NEW_CONN:
        src_index = *((unsigned int*)&pd[OFFSET_OF_DATA]);
        printf("process_packet_data NEW_CONN src_index=%u\n", src_index);
        clients[src_index].valid = true;
        break;
    case P_DATA_CODE_DEL_CONN:
        src_index = *((unsigned int*)&pd[OFFSET_OF_DATA]);
        printf("process_packet_data DEL_CONN src_index=%u\n", src_index);
        clients[src_index].valid = false;
        break;
    case P_DATA_CODE_TELEMETRY:
        t = (Telemetry*)&pd[OFFSET_OF_DATA];
        printf("process_packet_data TELEMETRY src_index=%u\n", t->src_index);
        if(clients[t->src_index].valid){
            memcpy(&clients[t->src_index].t, t, sizeof(Telemetry));
        }else{
            printf("process_packet_data TELEMETRY invalid client\n");
        }
        break;
    }
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
    // Telemetry t;
    // t.src_index = 0;
    // std::shared_ptr<char[]> sp = packet_data_telemetry(&t);

    // send a large raw data packet
    int size = 1024*1024;
    std::unique_ptr<char[]> data(new char[size]);
    std::shared_ptr<char[]> sp = packet_data_new(data.get(), size, P_DATA_CODE_RAW_DATA);
    econt->sendPacket(sp);
}

void scan_clients(void)
{
    int N_valid = 0;
    for(int i=0;i<N_CLIENT_ENDPOINTS;i++){
        if(clients[i].valid)
            N_valid++;
    }
    printf("scan_clients N_valid=%d\n", N_valid);
}

int main(int argc, char **argv)
{
    EndpointContext ec(N_THREADS, N_ENDPOINTS, client_recv_cb);

    for(int i=0;i<N_CLIENT_ENDPOINTS;i++){
        clients[i].valid = false;
    }

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
        //send_data_packet(econt);
        //scan_clients();
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000000/2;
        nanosleep(&ts, NULL);
    }
}
