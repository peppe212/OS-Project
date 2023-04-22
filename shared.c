#include "shared.h"

// Scrive "size" bytes su un descrittore di file
int writen(int fd, void *buffer, size_t size) {
    size_t nleft = size;
    ssize_t nread;
    char *buff_ptr = (char *) buffer;
    while (nleft > 0) {
        if ((nread = write((int) fd, buff_ptr, nleft)) == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (nread == 0)
            break;
        nleft -= nread;
        buff_ptr += nread;
    }
    return 0;
}


// Legge "size" bytes da un descrittore di file
int readn(int fd, void *buffer, size_t size) {
    size_t nleft = size;
    ssize_t nread;
    char *buff_ptr = (char *) buffer;
    while (nleft > 0) {
        if ((nread = read(fd, buff_ptr, nleft)) < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (nread == 0)          // EOF
            break;
        nleft -= nread;
        buff_ptr += nread;
    }
    return (int) (size - nleft);      // return >= 0
}


void scrivo_sul_socket(int fd, int msg) {
    int converted_n = htonl(msg);
    syscall(writen(fd, &converted_n, sizeof(converted_n)), "writen")
}


int leggo_dal_socket(int fd) {
    int received_n;
    syscall(readn(fd, &received_n, sizeof(received_n)), "readn")
    return ntohl(received_n);
}

