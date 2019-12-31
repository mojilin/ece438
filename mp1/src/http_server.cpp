/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <iostream>
#include <istream>



#define BACKLOG 10	 // how many pending connections queue will hold
#define BUFFER_SIZE 1000

using namespace std;

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    char request_buf[BUFFER_SIZE];
    char response_buf[BUFFER_SIZE];
    int numbytes;
    FILE *fp;

    if (argc != 2) {
        fprintf(stderr,"usage: server port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);
        memset(request_buf,'\0',BUFFER_SIZE);
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            recv(new_fd, request_buf, BUFFER_SIZE, 0);
            std::string request(request_buf);


            int temp1 = request.find_first_of("GET ");
            if (temp1 != 0) {
                request = "HTTP/1.1 400 Bad Request\r\n\r\n";
                if (send(new_fd, request.data(), request.size(), 0) == -1)
                    perror("send");
                exit(1);
            }

            request = request.substr(4);
            int temp2 = request.find_first_of(" HTTP");
            if (temp2 == request.npos) {
                request = "HTTP/1.1 400 Bad Request\r\n\r\n";
                if (send(new_fd, request.data(), request.size(), 0) == -1)
                    perror("send");
                exit(1);
            }

            string path = request.substr(1, temp2-1);
            fp = fopen(path.data(), "rb");

            if (fp == NULL) {
                request = "HTTP/1.1 404 Whoops, file not found\r\n\r\n";
                if (send(new_fd, request.data(), request.size(), 0) == -1)
                    perror("send");
                perror("fopen");
                exit(1);
            }

            request = "HTTP/1.1 200 OK\r\n\r\n";
            if (send(new_fd, request.data(), request.size(), 0) == -1){
                perror("send");
                exit(1);
            }
            memset(response_buf, '\0', BUFFER_SIZE);
            while ((numbytes = fread(response_buf, sizeof(char), BUFFER_SIZE, fp)) != 0) {
                if (send(new_fd, response_buf, numbytes, 0) == -1) {
                    perror("send");
                    exit(1);
                }
                memset(response_buf, '\0', BUFFER_SIZE);
            }
            fclose(fp);
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}


