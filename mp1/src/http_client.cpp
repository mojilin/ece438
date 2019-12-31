/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>

#include <arpa/inet.h>
#include <iostream>
#include <fstream>

#define MAXDATASIZE 1000 // max number of bytes we can get at once

using namespace std;
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    string url, protocol, host, port, path;

    ofstream outfile;

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    // parse url
    url = argv[1];
    int temp = url.find_first_of("//");
    protocol = url.substr(0, temp-1);
    url = url.substr(temp+2);
    temp = url.find('/');
    path = url.substr(temp);
    host = url.substr(0, temp);
    if (host.find(':') == host.npos) {
        port = "80";
    } else {
        temp = host.find(':');
        port = host.substr(temp+1);
        host = host.substr(0, temp);
    }
    cout << host << "\n" << port << "\n" << path << endl;

    if ((rv = getaddrinfo(host.data(), port.data(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    // http get connection
    auto request = "GET " + path + " HTTP/1.1\r\n"
                   + "User-Agent: Wget/1.12(linux-gnu)\r\n"
                   + "Host: " + host + ":" + port + "\r\n"
                   + "Connection: Keep-Alive\r\n\r\n";
    // send http request
    printf("%s\n", request.data());
    send(sockfd, request.c_str(), request.size(), 0);
    // receive http response
    outfile.open("output", ios::binary);
    int head = 1;
    while(1) {
        memset(buf, '\0', MAXDATASIZE);
        numbytes = recv(sockfd, buf, MAXDATASIZE, 0);
        if (numbytes > 0){
            if (head) {
                char* data_begin = strstr(buf, "\r\n\r\n") + 4;
                head = 0;
                outfile.write(data_begin,strlen(data_begin));
            } else {
                outfile.write(buf, sizeof(char) * numbytes);
            }

        } else {
            outfile.close();
            break;
        }
    }


    close(sockfd);

    return 0;
}

