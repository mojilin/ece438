//
// Created by linmoji on 2019-10-11.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include <iostream>
#include <queue>

using namespace std;

#define MSS 2000
#define BUFFER_SIZE 300
#define DATA 0
#define ACK 2
#define FIN 3
#define FIN_ACK 4
#define MAX_SEQ_NUMBER 100000
#define RTT 20*1000

//packet structure used for transfering
typedef struct{
    int 	data_size;
    int 	seq_num;
    int     ack_num;
    int     msg_type; //DATA 0 SYN 1 ACK 2 FIN 3 FINACK 4
    char    data[MSS];
}packet;

FILE* fp;
unsigned long long int num_pkt_total;
unsigned long long int bytesToRead;
//int file_point = 0;

// socket relevant
struct sockaddr_storage their_addr; // connector's address information
socklen_t addr_len = sizeof their_addr;
struct addrinfo hints, *recvinfo, *p;
int numbytes;

// Congestion Control
double cwnd = 1.0;
int ssthread = 64, dupAckCount = 0;
enum socket_state {SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY, FIN_WAIT};
int congetion_ctrl_state = SLOW_START;

// slide window
unsigned long long int seq_number;
char pkt_buffer[sizeof(packet)];
queue<packet> buffer;
queue<packet> wait_ack;


int getSocket(char *hostname, unsigned short int hostUDPport);
void openFile(char* filename, unsigned long long int bytesToTransfer);
void congestionControl(bool newACK, bool timeout);
int fillBuffer(int pkt_number);
void sendPkts(int socket);
void setSockTimeout(int socket);

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

    int socket = getSocket(hostname, hostUDPport);
    openFile(filename, bytesToTransfer);

    fillBuffer(BUFFER_SIZE);
    setSockTimeout(socket);
    sendPkts(socket);
    while (!buffer.empty() || !wait_ack.empty()) {
        if((numbytes = recvfrom(socket, pkt_buffer, sizeof(packet), 0, NULL, NULL)) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                perror("can not receive main ack");
                exit(2);
            }
            cout << "Time out, resend pkt " << wait_ack.front().seq_num << endl;
            memcpy(pkt_buffer, &wait_ack.front(), sizeof(packet));
            if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
                perror("Error: data sending");
                printf("Fail to send %d pkt", wait_ack.front().seq_num);
                exit(2);
            }
            congestionControl(false, true);
        } else {
            packet pkt;
            memcpy(&pkt, pkt_buffer, sizeof(packet));
            if (pkt.msg_type == ACK) {
                cout << "receive ack" << pkt.ack_num << endl;
                if (pkt.ack_num == wait_ack.front().seq_num) {
                    congestionControl(false, false);
                    if (dupAckCount == 3) {
                        ssthread = cwnd/2.0;
                        cwnd = ssthread + 3;
                        dupAckCount = 0;
                        congetion_ctrl_state = FAST_RECOVERY;
                        cout << "3 duplicate tp FAST_RECOVERY, cwnd = " << cwnd <<endl;
                        // resend duplicated pkt
                        memcpy(pkt_buffer, &wait_ack.front(), sizeof(packet));
                        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
                            perror("Error: data sending");
                            printf("Fail to send %d pkt", wait_ack.front().seq_num);
                            exit(2);
                        }
                        cout << "3 duplicate ACKs, resend pkt " << wait_ack.front().seq_num << endl;
                    }
                } else if (pkt.ack_num > wait_ack.front().seq_num) {
                    while (!wait_ack.empty() && wait_ack.front().seq_num < pkt.ack_num) {
                        congestionControl(true, false);
                        wait_ack.pop();
                    }
                    sendPkts(socket);
                }
            }
        }
    }
    fclose(fp);

    packet pkt;
    while (true) {
        pkt.msg_type = FIN;
        pkt.data_size=0;
        memcpy(pkt_buffer, &pkt, sizeof(packet));
        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("can not send FIN to sender");
            exit(2);
        }
        packet ack;
        if ((numbytes = recvfrom(socket, pkt_buffer, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&ack, pkt_buffer, sizeof(packet));
        if (ack.msg_type == FIN_ACK) {
            cout << "Receive the FIN_ACK." << endl;
            break;
        }
    }

}

int main(int argc, char** argv)
{
    unsigned short int udpPort;
    unsigned long long int numBytes;

    if(argc != 5)
    {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int)atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    return 0;
}

void setSockTimeout(int socket){
    struct timeval RTT_TO;
    RTT_TO.tv_sec = 0;
    RTT_TO.tv_usec = 2 * RTT;
    if (setsockopt(socket, SOL_SOCKET,SO_RCVTIMEO,&RTT_TO,sizeof(RTT_TO)) < 0) {
        fprintf(stderr, "Error setting socket timeout\n");
        return;
    }
}

void sendPkts(int socket) {

    int pkts_to_send =(cwnd - wait_ack.size()) <= buffer.size() ? cwnd - wait_ack.size() : buffer.size();
    if (cwnd - wait_ack.size() < 1) {
        memcpy(pkt_buffer, &wait_ack.front(), sizeof(packet));
        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("Error: data sending");
            printf("Fail to send %d pkt", wait_ack.front().seq_num);
            exit(2);
        }
        cout << "Sent pkt "<< wait_ack.front().seq_num << " cwnd = "<< cwnd << endl;
        return;
    }
    if (buffer.empty()) {
        cout << "no packet to send" << endl;
        return;
    }

    for (int i = 0; i < pkts_to_send; ++i) {
        memcpy(pkt_buffer, &buffer.front(), sizeof(packet));
        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("Error: data sending");
            printf("Fail to send %d pkt", buffer.front().seq_num);
            exit(2);
        }
        cout << "Sent pkt "<< buffer.front().seq_num << " cwnd = "<< cwnd << endl;
        wait_ack.push(buffer.front());
        buffer.pop();
    }
    fillBuffer(pkts_to_send);
}

int fillBuffer(int pkt_number) {
    if (pkt_number == 0) return 0;
    int byte_of_pkt;
    char data_buffer[MSS];
    int count = 0;
    for (int i = 0; bytesToRead!= 0 && i < pkt_number; ++i) {
        packet pkt;
        if (bytesToRead < MSS) {
            byte_of_pkt = bytesToRead;
        } else {
            byte_of_pkt = MSS;
        }
        int file_size = fread(data_buffer, sizeof(char), byte_of_pkt, fp);
        if (file_size > 0) {
            pkt.data_size = file_size;
            pkt.msg_type = DATA;
            pkt.seq_num = seq_number;
            memcpy(pkt.data, &data_buffer,sizeof(char)*byte_of_pkt);
            buffer.push(pkt);
            seq_number = (seq_number + 1) % MAX_SEQ_NUMBER;
        }
        bytesToRead -= file_size;
        count = i;
    }
    return count;
}

void congestionControl(bool newACK, bool timeout) {
    switch (congetion_ctrl_state) {
        case SLOW_START:
            if (timeout) {
                ssthread = cwnd/2.0;
                cwnd = 1;
                dupAckCount = 0;
                return;
            }
            if (newACK) {
                dupAckCount = 0;
                cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1;
            } else {
                dupAckCount++;
            }
            if (cwnd >= ssthread) {
                cout << "SLOW_START to CONGESTION_AVOIDANCE, cwnd = " << cwnd <<endl;
                congetion_ctrl_state = CONGESTION_AVOIDANCE;
            }
            break;
        case CONGESTION_AVOIDANCE:
            if (timeout) {
                ssthread = cwnd/2.0;
                cwnd = 1;
                dupAckCount = 0;
                cout << "CONGESTION_AVOIDANCE to SLOW_START, cwnd=" << cwnd <<endl;
                congetion_ctrl_state = SLOW_START;
                return;
            }
            if (newACK) {
                cwnd = (cwnd+ 1.0/cwnd >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+ 1.0/cwnd;
                dupAckCount = 0;
            } else {
                dupAckCount++;
            }
            break;
        case FAST_RECOVERY:
            if (timeout) {
                ssthread = cwnd/2.0;
                cwnd = 1;
                dupAckCount = 0;
                cout << "FAST_RECOVERY is SLOW_START, cwnd= " << cwnd <<endl;
                congetion_ctrl_state = SLOW_START;
                return;
            }
            if (newACK) {
                cwnd = ssthread;
                dupAckCount = 0;
                cout << "FAST_RECOVERY is CONGESTION_AVOIDANCE, cwnd = " << cwnd << endl;
                congetion_ctrl_state = CONGESTION_AVOIDANCE;
            } else {
                cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1;
            }
            break;
        default:
            break;
    }
}
int getSocket(char * hostname, unsigned short int hostUDPport) {
    int rv, sockfd;
    char portStr[10];
    sprintf(portStr, "%d", hostUDPport);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    memset(&recvinfo,0,sizeof recvinfo);
    if ((rv = getaddrinfo(hostname, portStr, &hints, &recvinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = recvinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: error opening socket");
            continue;
        }
        break;
    }
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    return sockfd;
}

void openFile(char* filename, unsigned long long int bytesToTransfer) {
    // Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }
    bytesToRead = bytesToTransfer;

    num_pkt_total = (unsigned long long int) ceil(bytesToRead * 1.0 / MSS);
    cout << num_pkt_total << endl;
}