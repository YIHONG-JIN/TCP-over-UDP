/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <queue>

using namespace std;

#define Slow_start 0
#define Timeout 1
#define Fast_recovery 2

#define MaxWindow 500
#define Maxdata 1400
#define Maxpacket 2000
#define DATA 0
#define ACK 1
#define FIN 2
#define FINACK 3
#define RTT 20*1000

struct sockaddr_in si_other;
struct sockaddr_storage their_addr; 
socklen_t addr_len = sizeof their_addr;
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
char pkt_buffer[sizeof(packet)];
char ACK_buffer[sizeof(ACK_packet)];
queue<packet> packet_list;
queue<packet> ACK_list;
//queue<packet> resend_list;

unsigned long long int total_bytes;
unsigned long long int sequence_num;
int Window_threshold = 80;
double window_size = 24.0;
struct addrinfo hints, *recvinfo, *p;
//struct timeval now;
//double period;
//double round_time_record = 0;
int congestion_num = 0;
int DUPACK = 0;
int stage_change;


FILE* fp;


// declare the help function
void fill_packet(int max_packet);
void sendPkts(int socket);
void resendPkts(int socket);
void congestion_control(int timeout);

void diep(char *s) {
    perror(s);
    exit(1);
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    if (bytesToTransfer <= 0){
        return;
    }
    
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);
    total_bytes = bytesToTransfer;
    

    
    /*
		 *socket: fill up the descriptor
		 *	Para:
		 *		int domain: IPV4 or IPV6
	 	 *		int type: socket stream or socket dgram
		 *		int potocol: tcp or udp
		 *	Return: socket descriptor or -1 for error

	*/
    int rv;
    char portStr[10];
    sprintf(portStr, "%d", hostUDPport);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    memset(&recvinfo,0,sizeof recvinfo);
    if ((rv = getaddrinfo(hostname, portStr, &hints, &recvinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
     // loop through all the results and bind to the first we can
    for(p = recvinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype,
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

    /*int sendto(int sockfd, const void *msg, int len, unsigned int flags,
     *    const struct sockaddr *to, socklen_t tolen);
     * Para: sockfd: the socket descriptor you want to write in
	 *       msg: the message send to the target 
	 * 		 len: length of the message
	 * 		 flag: set 0
     * return: number of bytes sent or -1 for error
     * 
    */

    // set up the timeout for recv to prevent endless waiting
    struct timeval Threshold;
    Threshold.tv_sec = 0;
    Threshold.tv_usec = 2 * RTT;
    
    if (setsockopt(s, SOL_SOCKET,SO_RCVTIMEO,&Threshold,sizeof(Threshold)) < 0) {
        fprintf(stderr, "fail to set the timeout\n");
    }
    
    fill_packet(Maxpacket);
    sendPkts(s);
    int numbytes; 
    
    // sending the data using GBN, if time out too receive ACK, resend the packet
    while(!packet_list.empty() || !ACK_list.empty() || total_bytes > 0 ){
        congestion_num = ACK_list.front().sequence_num;

        if((numbytes = recvfrom(s,ACK_buffer,sizeof(ACK_packet),0,NULL,NULL)) == -1){
            // this part will operate when a pakcet is lost in purpose
            cout << "cannot receive ACK" << endl;
            congestion_control(Timeout);
            resendPkts(s);
            //sendPkts(s);
        }else if (numbytes == 0){
            // time out, resend the packet
            congestion_control(Timeout);
            resendPkts(s);
            //sendPkts(s);
        }else{
            // receive the ACK
            ACK_packet pkt;
            memcpy(&pkt,ACK_buffer,sizeof(ACK_packet));
            if (pkt.msg_type == ACK && pkt.Ack_num >= ACK_list.front().sequence_num){
                
                while((pkt.Ack_num >= ACK_list.front().sequence_num) && !ACK_list.empty()){
                    cout << " packet " << ACK_list.front().sequence_num << " is received " << endl;
                    congestion_control(Slow_start);
                    ACK_list.pop();
                }
                if (packet_list.size() < window_size){
                    fill_packet(Maxpacket);
                }
                sendPkts(s);
                continue;
            }
            // DUPACK check and fast recovery
            if (pkt.Ack_num == congestion_num - 1){
                DUPACK += 1;
                congestion_control(Fast_recovery);
            }
            if (packet_list.size() < window_size){
                fill_packet(Maxpacket);
            }
            sendPkts(s);
            
        }

    }
    fclose(fp);
    // all data is sent, now finish
    packet finish;
    finish.sequence_num = sequence_num + 1;
    finish.msg_type = FIN;
    memcpy(pkt_buffer,&finish,sizeof(packet));
    printf("now finish sending\n");
    ACK_packet finish_ACK;
    
    while(1){
        if ((numbytes = sendto(s,pkt_buffer, sizeof(packet),0, p->ai_addr, p->ai_addrlen)) == -1){
            cout << "fail to sending finish " << endl;
            exit(2);
        }
        if ((numbytes = recvfrom(s, ACK_buffer, sizeof(ACK_packet), 0, (struct sockaddr*)&their_addr, &addr_len)) == -1) {
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&finish_ACK,ACK_buffer,sizeof(ACK_packet));
        if (finish_ACK.msg_type == FINACK){
            cout << "successfully disconnect" << endl;
            break;
        }

    }
    
	/* Send data and receive acknowledgements on s*/

    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}

/*
 * fill_packet: fill in the packets to keep the buffer full
 * Parameter: packets_to_fill: number of packets need to fill
 * Return value: None
 * Side effect: the queue of packet will be filled up with certain file
 * 
*/

void fill_packet(int packets_to_fill){
    if (packets_to_fill == 0 || total_bytes <= 0) return;
    int data_length;
    char data_buffer[Maxdata];
    for (int i = 0; total_bytes > 0 && i < packets_to_fill; i++) {
        packet pkt;
        // check whether the remaining bytes is enough
        if (total_bytes < Maxdata) {
            data_length = total_bytes;
        } else {
            data_length = Maxdata;
        }
        int file_size = fread(data_buffer, sizeof(char), data_length, fp);
        if (file_size > 0) {
            pkt.data_length = file_size;
            pkt.msg_type = DATA;
            pkt.sequence_num = sequence_num;
            memcpy(pkt.data, &data_buffer,sizeof(char)*data_length);
            packet_list.push(pkt);
            sequence_num = sequence_num + 1 ;
        }
        // the buffer is full now, or the file is done
        cout << "filling " << sequence_num << endl;
        total_bytes -= file_size;
    }
}

/*
 *   sendPkts:send packets to recevier and refill the buffer if the file is not finished
 *   Parameter: socket: the socket used for the sender
 *   Return value: None
 *   Side effect: send certain number of packet to the recevier
*/


void sendPkts(int socket){
    int send_pkts;
    if (packet_list.size() <= 0){
        return;
    }
    // check whether the number of packets remain is enough
    if (packet_list.size() > (window_size - ACK_list.size())){
        send_pkts = window_size - ACK_list.size();
    }else{
        send_pkts = packet_list.size();
    }
    if (send_pkts <= 0){
        return;
    }
    // while(!resend_list.empty() && send_pkts > 0){
    //     memcpy(pkt_buffer,&resend_list.front(),sizeof(packet));
    //     int numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen);
    //     //gettimeofday(&(resend_list.front().start),NULL);
    //     if(numbytes == -1){
    //         perror("Error: data sending");
    //         printf("Fail to send %d pkt", resend_list.front().sequence_num);
    //         exit(2);
    //     }
    //     cout << "Sent packets "<< resend_list.front().sequence_num << " window_size = "<< window_size << endl;
    //     ACK_list.push(resend_list.front());
    //     resend_list.pop();
    //     send_pkts -= 1;
    // }
    // if (send_pkts <= 0){
    //     return;
    // }
    for(int i = 0; i < send_pkts; i++){
        memcpy(pkt_buffer,&packet_list.front(),sizeof(packet));
        int numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen);
        //gettimeofday(&(packet_list.front().start),NULL);
        if(numbytes == -1){
            perror("Error: data sending");
            printf("Fail to send %d pkt", packet_list.front().sequence_num);
            exit(2);
        }
        cout << "Sent packets "<< packet_list.front().sequence_num << " window_size = "<< window_size << endl;
        ACK_list.push(packet_list.front());
        packet_list.pop();
    }
    // refill the packets if needed
    return;
}

void resendPkts(int socket){
    cout << "time out for packet " << ACK_list.front().sequence_num << endl;
    // resend all the packets for the GBN algorigthm
    // while(!ACK_list.empty()){
    //     if (ACK_record[ACK_list.front().sequence_num % MaxWindow] == 0){
    //         resend_list.push(ACK_list.front());
    //     }
    //     ACK_record[ACK_list.front().sequence_num % MaxWindow] = 0;
    //     ACK_list.pop();
        
    // }
    memcpy(pkt_buffer, &ACK_list.front(),sizeof(packet));
    int numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen);
    //gettimeofday(&(ACK_list.front().start),NULL);
    if(numbytes == -1){
        perror("Error: data sending");
        printf("Fail to send %d pkt", ACK_list.front().sequence_num);
        exit(2);
    }
    return;
}


void congestion_control(int timeout){
    if (timeout == Timeout){
        stage_change = Timeout;
        Window_threshold = (int) (window_size / 2);
        window_size = 24.0;
        DUPACK = 0;
        return;
    }
    
    if (timeout == Fast_recovery){
        if (stage_change == Fast_recovery){
            window_size += 1;
            return;
        }
        if (DUPACK == 3){
            Window_threshold = (int)(window_size/2) + 1;
            window_size = 24.0;
            stage_change = Fast_recovery;
            resendPkts(s);
            return;
        }
    }
    if (timeout == Slow_start){
        if (stage_change == Fast_recovery){
            window_size = 24.0;
            stage_change = Slow_start;
            return;
        }
//        if(window_size > Window_threshold){
//            window_size +=  1/(window_size);
//
//        }else{
//            window_size += 1;
//        }
        cout << "window_size = " << window_size << endl;
        cout << "Window_threshold = " << Window_threshold << endl;
        stage_change = Slow_start;
        DUPACK = 0;
        return;
    }
    
}










