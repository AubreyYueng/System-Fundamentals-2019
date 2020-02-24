#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>

#include "debug.h"
#include "protocol.h"
#include "csapp.h"
#include "errno.h"

// TODO: seems no necessary senario to errno

void set_time(BRS_PACKET_HEADER *hdr);
void host_to_network(BRS_PACKET_HEADER *hdr);
void network_to_host(BRS_PACKET_HEADER *hdr);

const char *types[] = {
    "NO_PKT", 
    "LOGIN", "STATUS", 
    "DEPOSIT", "WITHDRAW", 
    "ESCROW", "RELEASE", 
    "BUY", "SELL", "CANCEL", 
    "ACK", "NACK", 
    "BOUGHT", "SOLD", 
    "POSTED", "CANCELED", "TRADED"
};

int proto_send_packet(int fd, BRS_PACKET_HEADER *hdr, void *payload) {
    debug("Start to send package of type=%s to fd=%d", types[hdr->type], fd);
    if (hdr->type == BRS_NO_PKT) {
        debug("%s", "send no package");
        return -1;
    }
    
    uint16_t p_size = hdr->size;
    debug("payload size is %d", p_size);
    set_time(hdr);
    host_to_network(hdr);

    size_t h_size = sizeof(BRS_PACKET_HEADER);
    if (rio_writen(fd, hdr, h_size) != h_size) {
        debug("%s: %s\n", "rio_writen header error", strerror(errno));
        return -1;
    }

    if (p_size > 0) {
        if (rio_writen(fd, payload, p_size) != p_size) {
            debug("%s: %s\n", "rio_writen payload error", strerror(errno));
            return -1;
        }
    } else debug("%s", "sent payload is NULL");

    return 0;
}

int proto_recv_packet(int fd, BRS_PACKET_HEADER *hdr, void **payloadp) {
    ssize_t h_read;

    size_t h_size = sizeof(BRS_PACKET_HEADER);
    if ((h_read = rio_readn(fd, hdr, h_size)) < 0) {
        debug("%s: %s\n", "rio_readn header error", strerror(errno));
        return -1;
    }

    network_to_host(hdr);

    debug("Package of type=%s received from fd=%d", types[hdr->type], fd);
    if (hdr->type == BRS_NO_PKT) {
        debug("%s", "read no package");
        return -1;
    }

    uint16_t p_size = hdr->size;
    if (p_size > 0) {
        *payloadp = Malloc(p_size);
        int p_read;
        if ((p_read = rio_readn(fd, *payloadp, p_size)) != p_size) {
            // debug("%s: %s\n", "rio_readn payload error", strerror(errno));
            return -1;
        }
    } else debug("%s", "read p_size is 0 ");
    
    return 0;
}

void set_time(BRS_PACKET_HEADER *hdr) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    hdr->timestamp_sec = (uint32_t)t.tv_sec;
    hdr->timestamp_nsec = (uint32_t)t.tv_nsec;
}

void host_to_network(BRS_PACKET_HEADER *hdr) {
    hdr->size = htons(hdr->size);
    hdr->timestamp_sec = htonl(hdr->timestamp_sec);
    hdr->timestamp_nsec = htonl(hdr->timestamp_nsec);
}

void network_to_host(BRS_PACKET_HEADER *hdr) {
    hdr->size = ntohs(hdr->size);
    hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
    hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);
}