#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "packet.h"
#include "common.h"

#define STDIN_FD 0
#define RETRY 120 // millisecond

int next_seqno = 0;
int send_base = 0;
int window_size = 10;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer;
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;

// state variables
bool retransmit = false;
bool stopSending = false;
bool finished = false;

// filename
char *filename;

int start = -1;
int prevPkt;
int prevAck;
int pktCount = 0;

// for duplicate ACKs
int dAckCount = 0;
int dAck = 0;

// timeout ACK, to avoid redundancy
int tAck = 0;

tcp_packet *getPacket(int pcktNumber);
void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {
        for (int i = start + 1; i <= start + window_size; i++)
        {
            tcp_packet *pkt = getPacket(i);

            if (!stopSending && pkt->hdr.data_size == 0)
            {
                break;
            }

            if (pktCount > i)
            {
                if (sendto(sockfd, pkt, TCP_HDR_SIZE + get_data_size(pkt), 0,
                           (const struct sockaddr *)&serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }

                retransmit = true;
                prevPkt = i;
            }
        }
    }
}

void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

/*
 * init_timer: Initialize timeer
 * delay: delay in milli seconds
 * sig_handler: signal handler function for resending unacknoledge packets
 */
void init_timer(int delay, void (*sig_handler)(int))
{
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000; // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000; // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

tcp_packet *getPacket(int pcktNumber)
{
    char buffer[DATA_SIZE];

    FILE *f;
    f = fopen(filename, "r");

    int pktSize = 0;
    if (fseek(f, pcktNumber * DATA_SIZE, SEEK_SET) >= 0)
    {
        pktSize = fread(buffer, 1, DATA_SIZE, f);
    }

    tcp_packet *pkt = make_packet(pktSize);
    memcpy(pkt->data, buffer, pktSize);
    pkt->hdr.seqno = pcktNumber * DATA_SIZE;

    fclose(f);

    return pkt;
}

int main(int argc, char **argv)
{
    int portno, len;
    int next_seqno;
    char *hostname;
    char buffer[DATA_SIZE];
    FILE *fp;

    /* check command line arguments */
    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* initialize server server details */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0)
    {
        fprintf(stderr, "ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    // Stop and wait protocol
    init_timer(RETRY, resend_packets);
    next_seqno = 0;

    while (1)
    {
        sndpkt = getPacket(pktCount);
        len = sndpkt->hdr.data_size;
        if (len <= 0)
        {
            sndpkt = make_packet(0);
            prevAck = next_seqno;
            break;
        }
        send_base = next_seqno;
        next_seqno = send_base + len;

        pktCount++;
    }

    // when file is smaller than window size
    if (pktCount < window_size)
    {
        window_size = pktCount;
    }

    // sending packets based on the window size
    for (int i = 0; i < window_size; i++)
    {
        VLOG(DEBUG, "Sending packet %d to %s",
             send_base, inet_ntoa(serveraddr.sin_addr));
        /*
         * If the sendto is called for the first time, the system will
         * will assign a random port number so that server can send its
         * response to the src port.
         */
        tcp_packet *pkt = getPacket(i);
        if (sendto(sockfd, pkt, TCP_HDR_SIZE + get_data_size(pkt), 0,
                   (const struct sockaddr *)&serveraddr, serverlen) < 0)
        {
            error("sendto");
        }

        prevPkt = i;
    }

    start_timer();

    do
    {
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                     (struct sockaddr *)&serveraddr, (socklen_t *)&serverlen) < 0)
        {
            error("recvfrom");
        }

        recvpkt = (tcp_packet *)buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);

        if (dAck != prevAck && dAck == recvpkt->hdr.ackno)
        {
            dAckCount++;
            if (dAck != tAck && dAckCount >= 3)
            {
                VLOG(DEBUG, "Triple ACK RECEIVED");

                tAck = dAck;
                dAckCount = 0;
                resend_packets(SIGALRM);
            }
        }

        else
        {
            // END TRANSMISSION
            if (recvpkt->hdr.ackno == prevAck && stopSending)
            {
                tcp_packet *pkt = getPacket(pktCount * DATA_SIZE);

                for (int i = 0; i < 5; i++)
                {
                    if (sendto(sockfd, pkt, TCP_HDR_SIZE + get_data_size(pkt), 0,
                               (const struct sockaddr *)&serveraddr, serverlen) < 0)
                    {
                        error("sendto");
                    }
                }

                finished = true;
            }

            else if (dAck <= recvpkt->hdr.ackno)
            {
                start = ceil(recvpkt->hdr.ackno / DATA_SIZE) - 1;

                if (start > prevPkt)
                {
                    prevPkt = start;
                }

                dAck = recvpkt->hdr.ackno;
                dAckCount = 0;

                for (int i = prevPkt + 1; i <= start + window_size; i++)
                {

                    tcp_packet *pkt = getPacket(i);

                    if (pkt->hdr.data_size == 0)
                    {
                        stopSending = true;
                    }
                    else
                    {
                        if (!retransmit)
                        {
                            init_timer(RETRY, resend_packets);
                        }

                        stop_timer();

                        VLOG(DEBUG, "Sending packet %d to %s",
                             pkt->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

                        if (sendto(sockfd, pkt, TCP_HDR_SIZE + get_data_size(pkt), 0,
                                   (const struct sockaddr *)&serveraddr, serverlen) < 0)
                        {
                            error("sendto");
                        }

                        start_timer();

                        retransmit = false;
                        prevPkt = i;
                    }
                }
            }
        }
    } while (!finished);

    free(sndpkt);

    return 0;
}
