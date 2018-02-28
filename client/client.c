// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h> //for read
#include <arpa/inet.h>//for inet_pton
#include <errno.h>

#define servPORT 8090
#define FILENAME
ssize_t rio_writen(int fd, void* usrbuf, size_t n);
ssize_t rio_readn(int fd, void* usrbuf, size_t n);
int main(int argc, char const *argv[])
{
    struct sockaddr_in address;
    struct sockaddr_in serv_addr;
    //-----
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    //----
    int sock = 0, valread;
    char *hello = "Hello from client";
    char buffer[1024] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(servPORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    //connect step
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    nread = getline(&line, &len, stdin);
    rio_writen(sock, line, nread);
    //send(sock , hello , strlen(hello) , 0 );
    printf("Hello message sent\n");
    valread = read(sock , line, 1024);
    printf("%s\n",line);
    return 0;
}

/*
//On successful completion should return 0
//recvPORT should be a cstyle string, rcv PORT from server
//ideally, the ftp server should have provided the size of the bytes beforehand
//(1)consider breaking up this function into 2 st that first half deals with
//creating socket + port,
//sendin PORT needs to know which port we are receiving on so that it
//can getaddrinfo(), create socket, connect() that socket and sendfile() to that socket
//from which we can recieve
//(2) to avoid code repetition, the create socket part should have it's own function
int receive_file(char* sourcePORT, char* rcvFILENAME, size_t file_size, int newFD){
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
    //create connection between Server sendfile PORT and our receive file PORT
    struct addrinfo hints, *servinfo, *p;
    int file_sockfd, status;
    char* rcvBUFFER;
    ssize_t n_IO;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(argv[1], sourcePORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "receive_file getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((file_sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
            fprintf(stderr, "receive_file client: socket() error\n");
            continue;
        }
        if (connect(file_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(file_sockfd);
            fprintf(stderr, "receive_file client: connect() error\n");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return -1;
    }
    freeaddrinfo(servinfo);
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
    //Transfer file from ftp server to local file
    if((rcvBUFFER = malloc(file_size) == NULL){
        fprintf(stderr, "receive_file client: malloc() rcvBUFFER error\n");
        return -1;
    }
    //Robust read from socket to buffer;
    if ((n_IO = rio_readn(file_sockfd, rcvBUFFER, file_size)) == -1){
        fprintf(stderr, "read to rcvBUFFER from file_sockfd error\n");
        return -1;
    }

    //Robust write;
    if ((n_IO = rio_writen(newFD, rcvBUFFER, file_size)) == -1){
        fprintf(stderr, "read to rcvBUFFER from file_sockfd error\n");
        return -1;
    }


    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
    //clean up and return;
    free(rcvBUFFER);
    close(file_sockfd);//do error checking
    return 0;//successful completion :-)
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
}
*/
//transfers n bytes from file fd to userbuf
//Robust Read
//can only return short count if it encounters EOF
ssize_t rio_readn(int fd, void* usrbuf, size_t n){
    size_t nleft = n;
    ssize_t nread;
    char* bufp = usrbuf;

    while(nleft > 0){
        nread = read(fd, bufp, nleft);
        //restart if interrupted by sig handler return
        //call read() again, otherwise,errno set by read()
        if(nread < 0){
            if(errno == EINTR) nread = 0;
            else return -1;
        }
        //EOF, succesful reading
        else if(nread == 0) break;
        nleft -= nread;
        bufp += nread;//update bufp pointer to avoid overwriting
    }
    return (n - nleft);
}

//transfers n bytes from usrbuf to file fd
//Robust: never returns a short count
//Returns number of of bytes transfered
ssize_t rio_writen(int fd, void* usrbuf, size_t n){
    size_t nleft = n;
    ssize_t nwritten;
    char* bufp = usrbuf;

    while(nleft > 0){
        nwritten = write(fd, bufp, nleft);
        if(nwritten <= 0){
            if(errno == EINTR) nwritten = 0;
            else return -1;
        }
        nleft -=nwritten;
        bufp += nwritten;
    }

    return n;
}










































//END
