/*
Implements active FTP

*/
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h> //for read, getcwd()
#include <arpa/inet.h>//for inet_pton
#include <errno.h>
#include <netdb.h>
#include <inttypes.h>
#include <dirent.h>//for DIR cmds sections
#include <wordexp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define servPORT "8090"
#define servIP "127.0.0.1"
#define BUF_SIZE 1024
#define RESET_BUF(buf) memset(buf, 0, BUF_SIZE)
ssize_t rio_writen(int fd, void* usrbuf, size_t n);
ssize_t rio_readn(int fd, void* usrbuf, size_t n);

#define DIR_PATH_SIZE 2048

void* get_in_addr(struct sockaddr *sa){
    return &(((struct sockaddr_in*)sa)->sin_addr);
}

int get_connection_sock(char* serv_IP, char* serv_PORT);

int procedure_get_file(int cmdSOCK, FILE* file_fp, char* serv_IP, char* serv_PORT);
int procedure_put_file(int cmdSOCK, int fileFD, char* serv_IP, char* serv_PORT);
int procedure_list_files(char* current_dir);
void procedure_change_directory(char * current_directory, char * new_directory);
// path =  ./f.txt

int main(){
    int cmdSOCK, ret, status;
    //cmdline vars
    char buffer[BUF_SIZE];
    char args[2][10];
    //sending cmds
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    //for file transfer
    char strn_file_port[50];
    intmax_t filesize;
    //for directory commands
    char current_dir[DIR_PATH_SIZE];
    getcwd(current_dir, DIR_PATH_SIZE);

    if((cmdSOCK= get_connection_sock(servIP, servPORT)) < 0){
        fprintf(stderr, "%s\n", "Cannot connect to FTP Server");
        exit(1);
    }

    while(1){
        //credits to NABIL for cmd line parsing:
        //source: https://nabilrahiman.hosting.nyu.edu/simple-command-line-parsing-in-c/
        printf("ftp> ");
        fgets(buffer, sizeof(buffer), stdin);
        ret = sscanf(buffer, "%s %s", args[0], args[1]);
        if(strcmp(args[0], "QUIT") == 0) break;//ideally, cliet should send quit command to server so that it can clean up, but hey, deadline  ¯\_(ツ)_/¯

        else if(strcmp(args[0], "PUT") == 0){
            //check if we have file

            int fileFD, n_file_port = 0;
            struct stat file_stat;
            char temp[20];
            //RESET_BUF(buffer);
            fileFD = open(args[1], O_RDONLY, 0);
            //cint fileFD = open(args[1], O_RDONLY, 0);
            if((fstat(fileFD, &file_stat)) < 0){
                printf("Error: Eo such file\n\n");
                continue;
            }

            sprintf(temp, "%jd", (intmax_t)file_stat.st_size);
            strcat(buffer, temp);

            rio_writen(cmdSOCK, buffer, strlen(buffer));
            printf("Setting up connection for file transfer ...\n");
            RESET_BUF(buffer);
            nread = read(cmdSOCK, buffer, BUF_SIZE);
            if(nread == 0) {printf("Connection closed by server\n");break;}
            if(strcmp(buffer, "530") == 0){
                printf("NOT AUTHORIZED\n\n"); continue;
            }
            //at this point we suppose server has postive reply
            sscanf(buffer, "%d", &n_file_port);
            if (n_file_port == 0){
                //if file is available and can be sent ftp server will always assign a nonzero port number
                printf("FTP: No such file\n\n");
            }else{
                sprintf(strn_file_port, "%d", n_file_port);//convert
                printf("UPLOADING FILE_SIZE: %jd PORT: %s\n", (intmax_t)file_stat.st_size, strn_file_port);
                //server will reply with file size and port, in buffer
                status = procedure_put_file(cmdSOCK, fileFD, servIP, strn_file_port);
                //do error checking for get_file_procedure
            }

            //receive

            //receive port number for file connection



        }
        else if(strcmp(args[0], "GET") == 0){
            int n_file_port = 0;
            //TODO FIRST CHECK IF WE CAN OPEN FILE, BEFORE SENDING COMMAND TO SERVER
            //TRY TO OPEN FILE FIRST, set to write
            FILE* file_fp = fopen(args[1], "a");
            if(file_fp == NULL){
                perror("Error: ");
                continue;
            }

            //If successful, send GET command to FTP
            rio_writen(cmdSOCK, buffer, strlen(buffer));
            RESET_BUF(buffer);

            //FTP will reply with info that'll help set up the file transfer
            nread = read(cmdSOCK, buffer, BUF_SIZE);
            if(nread == 0) {printf("Connection closed by server\n");break;}
            if(strcmp(buffer, "530") == 0){
                printf("530 NOT AUTHORIZED\n\n"); continue;
            }
            //at this point we suppose server has postive reply
            sscanf(buffer, "%jd %d", &filesize, &n_file_port);
            if (n_file_port == 0){
                //if file is available and can be sent ftp server will always assign a nonzero port number
                printf("FTP: No such file\n\n");
            }else{
                sprintf(strn_file_port, "%d", n_file_port);//convert
                fprintf(stdout, "RECEIVING FILE_SIZE: %jd PORT: %s\n", filesize, strn_file_port);
                //server will reply with file size and port, in buffer
                status = procedure_get_file(cmdSOCK, file_fp, servIP, strn_file_port);
                //do error checking for get_file_procedure
            }
            continue;
        }
        else if(strcmp(args[0], "!LS") == 0){
            //TODO make it more like terminal's CD, ie less simplistic
            //currently can only support LS for CWD
            procedure_list_files(current_dir);
            continue;
        }
        else if(strcmp(args[0], "!PWD") == 0){
            printf("%s\n\n",current_dir);
            continue;
        }
        else if(strcmp(args[0], "!CD") == 0){
            procedure_change_directory(current_dir, args[1]);
            continue;
        }
        else{//USER PASS other cmds
            rio_writen(cmdSOCK, buffer, strlen(buffer));
            RESET_BUF(buffer);

            nread = read(cmdSOCK, buffer, BUF_SIZE);
            if(nread == 0) {printf("Connection closed by server\n");break;}
            if(strcmp(buffer, "530") == 0){
                printf("NOT AUTHORIZED\n\n"); continue;
            }
            printf("%s\n\n", buffer);
        }
    }


    printf("DUNZO\n");


    close(cmdSOCK);
    return 0;
}

//Given server IP, and server PORT, sets up a connection socket
//returns socket fd,
//on failure, returns -1;
int get_connection_sock(char* serv_IP, char* serv_PORT){
    int cmdSOCK, status;
    struct sockaddr_in serv_addr;
    struct addrinfo hints, *servINFO, *p;
    char s[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if((status = getaddrinfo(serv_IP, serv_PORT, &hints, &servINFO)) != 0){
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(status));
        return -1;
    }

    for(p = servINFO; p != NULL; p = p->ai_next) {
        if ((cmdSOCK = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) < 0) {
            perror("socket failed\n");
            continue;
        }
        if(connect(cmdSOCK, p->ai_addr, p->ai_addrlen) == -1){
            close(cmdSOCK);
            perror("client: connect");
            continue;
        }
        break;
    }
    if(p == NULL){
        fprintf(stderr, "Client: failed to connect\n");
        return -1;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    printf("client: connecting to %s, port: %s\n\n",s, serv_PORT);
    freeaddrinfo(servINFO);//clean up

    return cmdSOCK;
}



int procedure_put_file(int cmdSOCK, int fileFD, char* serv_IP, char* serv_PORT){
    int new_fileSOCK;
    //given the port from the server, client attempts to connect to that port
    if((new_fileSOCK= get_connection_sock(serv_IP, serv_PORT)) < 0){
        fprintf(stderr, "%s\n", "Cannot connect to FTP Server");
        return -1;
    }

    //test msg:
    char* reply = "LI LI LI lick my balls!!!";
    rio_writen(new_fileSOCK, reply, strlen(reply));

    close(new_fileSOCK);
    return 1;
}


//get file should also recieve file name from argv[1
//client should be able to parse reply from SERVER
int procedure_get_file(int cmdSOCK, FILE* file_fp, char* serv_IP, char* serv_PORT){
    char file_buffer[BUF_SIZE];
    int new_fileSOCK;
    unsigned bytes_downloaded;

    //client side will receive file transfer PORT and file size, b
    //Suggestion: do all buffer stuff in main loop,


    //given the port from the server, client attempts to connect to that port
    if((new_fileSOCK= get_connection_sock(serv_IP, serv_PORT)) < 0){
        fprintf(stderr, "%s\n", "Cannot connect to FTP Server");
        return -1;
    }

    do{
        bytes_downloaded = read(new_fileSOCK, file_buffer, BUF_SIZE);
        //printf("bytes downloaded: %d\n", bytes_downloaded);
        fwrite(file_buffer, sizeof(char), bytes_downloaded,file_fp);
        //fputs(file_buffer, file_fp);
    }while(bytes_downloaded >  0);
    if((int)bytes_downloaded < 0) perror("Error downloading file");
    if(bytes_downloaded == 0)printf("Completed Downloading\n");
    fclose(file_fp);
    //client closes SOCK
    close(new_fileSOCK);
    //return, at any given moment, if any error occurs
    return 1;
}

int procedure_list_files(char* current_dir){
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (current_dir)) != NULL) {
        //print all the files and directories within directory
        while ((ent = readdir (dir)) != NULL) {
            printf ("%s\n", ent->d_name);
        }
        closedir (dir);
        printf("\n");
        return 1;
    }else{
        perror ("could not open directory\n\n");
        return 0;
    }
}

void procedure_change_directory(char * current_dir, char * new_dir){
    //credits: github/mslos
	char new_path[DIR_PATH_SIZE];
    DIR* dir;
	if(new_dir[0] == '/') {
		strcpy(new_path,new_dir);
	}
	else if (new_dir[0]=='~') {//shell-tilder expansion for tilde
	   	wordexp_t p;//for
	   	wordexp(new_dir, &p, 0);
	    strcpy(new_path,p.we_wordv[0]);
	    wordfree(&p);
	}
	else
		strcat(strcat(strcpy(new_path,current_dir),"/"),new_dir);

	if ((dir = opendir(new_path))) {
		realpath(new_path,current_dir);
        printf("%s\n\n",current_dir);
        //printf("%s\n\n", new_dir);
	    closedir(dir);
	}
	else{
        printf("CD error\n\n");
    }
}
































































//Credits: CSAPP, Bryant, O'Hallaron
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

//Credits: CSAPP, Bryant, O'Hallaron
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
