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
#include <iostream>
#include <queue>

using namespace std;

#define MAXDATASIZE 2000000
#define MaxWindow 500
#define DATA 0
#define ACK 1
#define FIN 2
#define FINACK 3
#define Maxdata 1400

struct sockaddr_in si_me, si_other;
int s, slen;
typedef struct{
    int 	data_length;
    int 	sequence_num;
    int     msg_type; //DATA 0 ACK 1 FIN 2 FINACK 3
    //struct timeval start;
    char    data[Maxdata];
}packet;

typedef struct 
{
    int     msg_type; //DATA 0 ACK 1 FIN 2 FINACK 3
    int     Ack_num;
}ACK_packet;



void diep(char *s) {
    perror(s);
    exit(1);
}



void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        printf("cannot find socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    /* int bind(int sockfd, struct sockaddr *my_addr, int addrlen): associate the socket with port in local machine
		 * Para: sockfd: socket descriptor from the socket()
		 *       my_addr: include my own port an IP address
		 * 		 addrlen: len in bits of that address
		 * Return: -1 for error
		*/

    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1){
        printf("fail to bind");

    }

	/* Now receive data and send acknowledgements */    
    FILE* fp = fopen(destinationFile,"wb");
    char file_buf[MAXDATASIZE];
    char receiver_buf[MaxWindow*Maxdata];
    struct sockaddr_in sender;
    socklen_t sendlen = sizeof(sender);
    int ACK_index[MaxWindow];
    int data_length_list[MaxWindow];
    //int last_ACK = 0;
    int Next_ACK_index = 0;
    char packet_buf[sizeof(packet)];
    char ACK_buf[sizeof(ACK_packet)];
    
    int numbytes;
    //int recvfrom(int sockfd, void *buf, int len, unsigned int flags,
    //    struct sockaddr *from, int *fromlen);
    printf("now begin to receive\n");
    while(1){    
        if ((numbytes = recvfrom(s,packet_buf,sizeof(packet),0,(struct sockaddr*) &sender,&sendlen)) <= 0 ){
            printf("receive nothing\n");
            exit(2);
        }
        packet pkt;
        ACK_packet ack_back;
        memcpy(&pkt,packet_buf,sizeof(packet));
        // now receives the data with recording window
        if (pkt.msg_type == DATA){
            
            if (pkt.sequence_num == Next_ACK_index){

                memcpy(&file_buf[Next_ACK_index*Maxdata % (MAXDATASIZE - Maxdata)], &pkt.data , pkt.data_length);
                fwrite(&file_buf[Next_ACK_index*Maxdata % (MAXDATASIZE - Maxdata)],sizeof(char),pkt.data_length,fp);
                cout << " recevied " << pkt.data_length << " bytes " << endl;
                Next_ACK_index++;
                
                // ack_back.Ack_num = Next_ACK_index - 1;
                // ack_back.msg_type = ACK;
                // memcpy(ACK_buf,&ack_back,sizeof(ACK_packet));
                // sendto(s,ACK_buf,sizeof(ACK_packet),0,(struct sockaddr*)&sender,sendlen);
                // cout << " ACK " << Next_ACK_index - 1 << " is sent " << endl;
                while(ACK_index[Next_ACK_index % MaxWindow] == 1){
                    fwrite(&receiver_buf[(Next_ACK_index % MaxWindow)*Maxdata ],sizeof(char),data_length_list[Next_ACK_index % MaxWindow],fp);
                    ACK_index[Next_ACK_index % MaxWindow] = 0;
                    Next_ACK_index++;
                }
            }else if(pkt.sequence_num > Next_ACK_index) {
                int index = pkt.sequence_num;
                if (ACK_index[index % MaxWindow] == 0) {
                    ACK_index[index % MaxWindow] = 1;
                    data_length_list[index % MaxWindow] = pkt.data_length;
                    memcpy(&receiver_buf[(index % MaxWindow) * Maxdata], &pkt.data, pkt.data_length);
                }
            }
            ack_back.Ack_num = Next_ACK_index - 1;
            ack_back.msg_type = ACK;
            memcpy(ACK_buf,&ack_back,sizeof(ACK_packet));
            sendto(s,ACK_buf,sizeof(ACK_packet),0,(struct sockaddr*)&sender,sendlen);
            cout << " ACK " << pkt.sequence_num << " is sent " << endl;

        }else{
            // if the packet is to close the connection, close the connection and quit
            if (pkt.msg_type == FIN){
                ACK_packet finish;
                finish.msg_type= FINACK;
                finish.Ack_num = Next_ACK_index;
                memcpy(ACK_buf,&finish,sizeof(ACK_packet));
                sendto(s, ACK_buf, sizeof(ACK_packet), 0, (struct sockaddr *)&sender,sendlen);
                cout << "break down the connection " << endl;
                break;
            }
        }
    }
    //fclose(fp);
    close(s);
    return;
}

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

