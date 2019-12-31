/*
 * File:   receiver_main.c
 * Author:
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include "string"
#include <cstring>
#include <iostream>
#include <netdb.h>

#define DATA_SIZE 2000
#define BUFF_SIZE 600000
#define TOT_COUNT 300
#define ACK 2
#define FIN_ACK 4
#define DATA 0
#define FIN 3
using namespace std;

typedef struct{
    int 	data_size;
    int 	seq_num;
    int     ack_num;
    int     msg_type;
    char data[DATA_SIZE];
}packet;

struct sockaddr_in si_me, si_other;
int s, slen,recvbytes;
char buf[sizeof(packet)];
struct sockaddr_in sender_addr;
socklen_t addrlen;



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    slen = sizeof (si_other);
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        printf("Socket error\n");
        exit(1);
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (::bind(s, (struct sockaddr*)&si_me, sizeof (si_me)) < 0){
        printf("Binding error\n");
        exit(1);
    }
    /* Now receive data and send acknowledgements */

    FILE* fp = fopen(destinationFile,"wb");

    char file_buffer [BUFF_SIZE];

    int nextACK = 0;
    int already_ACK [TOT_COUNT];
    int data_size [TOT_COUNT];
    for (int i=0;i<TOT_COUNT;i++){
        already_ACK[i]=0;
        data_size[i]=DATA_SIZE;
    }
    addrlen = sizeof sender_addr;
    int cur_idx = 0;
    while(1){
        recvbytes = recvfrom(s, buf, sizeof(packet), 0, (struct sockaddr*)&sender_addr, &addrlen);
        if (recvbytes<=0){
            fprintf(stderr, "Connection closed\n");
            exit(2);
        }
        packet pkt;
        memcpy(&pkt,buf,sizeof(packet));
        cout << "receive pkt" << pkt.seq_num<< " type " << pkt.msg_type<< endl;
        if (pkt.msg_type==DATA){
            if(pkt.seq_num==nextACK){
                memcpy(&file_buffer[cur_idx*DATA_SIZE], &pkt.data , pkt.data_size);
                fwrite(&file_buffer[cur_idx*DATA_SIZE],sizeof(char),pkt.data_size,fp);
                cout << "write pkt "<< pkt.seq_num << endl;
                nextACK++;
                cur_idx  = (cur_idx+1)%TOT_COUNT;
                while(already_ACK[cur_idx]==1){
                    fwrite(&file_buffer[cur_idx*DATA_SIZE],sizeof(char),data_size[cur_idx],fp);
                    cout << "write index "<< cur_idx << endl;
                    already_ACK[cur_idx] = 0;
                    cur_idx  = (cur_idx+1)%TOT_COUNT;
                    nextACK++;
                }
            }else if(pkt.seq_num>nextACK){
                int ahead_idx = (cur_idx+pkt.seq_num-nextACK)%TOT_COUNT;
                for (int i=0;i<pkt.data_size;i++) {
                    file_buffer[ahead_idx*DATA_SIZE+i] = pkt.data[i];
                }
                already_ACK[ahead_idx] = 1;
                data_size[ahead_idx] = pkt.data_size;
            }
            packet ack;
            ack.msg_type=ACK;
            ack.ack_num = nextACK;
            ack.data_size = 0; // data size is 0 since we are sending ack
            memcpy(buf,&ack,sizeof(packet));
            sendto(s, buf, sizeof(packet), 0, (struct sockaddr *) &sender_addr,addrlen);
            cout << "sent ack" << ack.ack_num << endl;
        }else if(pkt.msg_type== FIN){
            //fwrite(file_buffer,sizeof(char),BUFF_SIZE,fp);
            packet ack;
            ack.msg_type=FIN_ACK;
            ack.ack_num = nextACK;
            ack.data_size = 0;
            memcpy(buf,&ack,sizeof(packet));
            sendto(s, buf, sizeof(packet), 0, (struct sockaddr *) &sender_addr,addrlen);
            break;
        }
    }//while(1)
    close(s);
    printf("%s received.", destinationFile);
    return;
}//function

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}
