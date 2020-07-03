//
// Created by gg on 04.07.2020.
//

////---cat rawtcp.c---
//
//// Run as root or SUID 0, just datagram no data/payload
//


#include <unistd.h>

#include <stdio.h>

#include <sys/socket.h>

#include <netinet/ip.h>

#include <netinet/tcp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


//
//// Packet length
//
//#define PCKT_LEN 8192
//
//
//
//// May create separate header file (.h) for all
//
//// headers' structures
//
//// IP header's structure
//
//struct ipheader {
//
//    unsigned char      iph_ihl:5, /* Little-endian */
//
//            iph_ver:4;
//
//    unsigned char      iph_tos;
//
//    unsigned short int iph_len;
//
//    unsigned short int iph_ident;
//
//    unsigned char      iph_flags;
//
//    unsigned short int iph_offset;
//
//    unsigned char      iph_ttl;
//
//    unsigned char      iph_protocol;
//
//    unsigned short int iph_chksum;
//
//    unsigned int       iph_sourceip;
//
//    unsigned int       iph_destip;
//
//};
//
//
//
///* Structure of a TCP header */
//
//struct tcpheader {
//
//    unsigned short int tcph_srcport;
//
//    unsigned short int tcph_destport;
//
//    unsigned int       tcph_seqnum;
//
//    unsigned int       tcph_acknum;
//
//    unsigned char      tcph_reserved:4, tcph_offset:4;
//
//    // unsigned char tcph_flags;
//
//    unsigned int
//
//            tcp_res1:4,      /*little-endian*/
//
//            tcph_hlen:4,     /*length of tcp header in 32-bit words*/
//
//            tcph_fin:1,      /*Finish flag "fin"*/
//
//            tcph_syn:1,       /*Synchronize sequence numbers to start a connection*/
//
//            tcph_rst:1,      /*Reset flag */
//
//            tcph_psh:1,      /*Push, sends data to the application*/
//
//            tcph_ack:1,      /*acknowledge*/
//
//            tcph_urg:1,      /*urgent pointer*/
//
//            tcph_res2:2;
//
//    unsigned short int tcph_win;
//
//    unsigned short int tcph_chksum;
//
//    unsigned short int tcph_urgptr;
//
//};
//
//
//
//// Simple checksum function, may use others such as Cyclic Redundancy Check, CRC
//
//unsigned short csum(unsigned short *buf, int len)
//
//{
//
//    unsigned long sum;
//
//    for(sum=0; len>0; len--)
//
//        sum += *buf++;
//
//    sum = (sum >> 16) + (sum &0xffff);
//
//    sum += (sum >> 16);
//
//    return (unsigned short)(~sum);
//
//}




#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#define PCKT_LEN 8192

unsigned short csum(unsigned short *buf, int nwords)
{
    unsigned long sum;
    for(sum=0; nwords>0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

int main(int argc, char const *argv[])
{
    if (argc != 5) {
        printf("Error: Invalid parameters!\n");
        printf("Usage: %s <source hostname/IP> <source port> <target hostname/IP> <target port>\n", argv[0]);
        exit(1);
    }

    u_int16_t src_port, dst_port;
    u_int32_t src_addr, dst_addr;

    src_addr = inet_addr(argv[1]);
    dst_addr = inet_addr(argv[3]);
    src_port = atoi(argv[2]);
    dst_port = atoi(argv[4]);

    int sd;
    char buffer[PCKT_LEN];

    ::memset(buffer, 'A', PCKT_LEN);

    struct iphdr*  ip  = (struct iphdr*) buffer;
    struct udphdr* udp = (struct udphdr*)(buffer + sizeof(struct iphdr));

    struct sockaddr_in sin;
    int        one = 1;
    const int* val = &one;

    memset(buffer, 0, PCKT_LEN);
    int SIZE = 64;

    // create a raw socket with UDP protocol
    sd = socket(PF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sd < 0) {
        perror("socket() error");
        exit(2);
    }
    printf("OK: a raw socket is created.\n");

    // inform the kernel do not fill up the packet structure, we will build our own
    if(setsockopt(sd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("setsockopt() error");
        exit(2);
    }
    printf("OK: socket option IP_HDRINCL is set.\n");

    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(dst_port);
    sin.sin_addr.s_addr = dst_addr;

    // fabricate the IP header
    ip->ihl      = 5;
    ip->version  = 4;
    ip->tos      = 16; // low delay
    ip->tot_len  = sizeof(struct iphdr) + sizeof(struct udphdr) + SIZE;
    ip->id       = htons(54321);
    ip->ttl      = 64; // hops
    ip->protocol = 17; // UDP
    // source IP address, can use spoofed address here
    ip->saddr = src_addr;
    ip->daddr = dst_addr;

    // fabricate the UDP header
    udp->source = htons(src_port);
    // destination port number
    udp->dest = htons(dst_port);
    udp->len = htons(sizeof(struct udphdr));

    // calculate the checksum for integrity
    ip->check = csum((unsigned short *)buffer,
                     sizeof(struct iphdr) + sizeof(struct udphdr));

    if (sendto(sd, buffer, ip->tot_len, 0,
               (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("sendto()");
        exit(3);
    }
    printf("OK: one packet is sent.\n");

    close(sd);
    return 0;
}