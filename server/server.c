// server side

#include <stdio.h> //for printf
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h> //for read
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <fcntl.h>
#include <assert.h>
#include <dirent.h>//for DIR cmds sections
#include <wordexp.h>


#include "users.h"

#define _PORT "8090"
#define BUF_SIZE 1024
#define RESET_BUF(buf) memset(buf, 0, BUF_SIZE)
#define LISTEN_QUEUE_SIZE 10 //connections queue for listen(),

//forward decl
int get_listensockfd(char* PORT);
int get_port_from_sock(int sock);


//once a user establishes a connection, their socket becomes assc with this obj
typedef struct{
    int sock;
    char* name;
    char* pass;
    int authenticated;
    Account* user_p;
} client_obj;


typedef struct {
    int lsock;
    int fsock;
    int mode;//either GET(1) or PUT(2)
    Account* user_p;
}ft_main_obj;

void reset_client_obj(client_obj* client){
    if(client){
        client->sock = -1;
        client->name = NULL;
        client->pass = NULL;
        client->authenticated = 0;
        client->user_p = NULL;
    }
}


void reset_ft_struct_obj(ft_main_obj* ft_l_obj_p){
    if(ft_l_obj_p){
        ft_l_obj_p->lsock = -1;
        ft_l_obj_p->user_p = NULL;
        ft_l_obj_p->fsock = -1;
        ft_l_obj_p->mode = 0;
    }
}

//user commands
void list_files_cmd(client_obj* client);
void pwd_cmd(client_obj* client);
void user_cmd(char* name_arg, Account Accounts_arr[], client_obj* client);
void pass_cmd(char* pass_arg, client_obj* client);

//file transfer functions
int setup_ft_listening_sock(client_obj* client, fd_set* ft_allset_listen_p,
                            int* ft_maxfd_listen_p, int* ft_maxi,
                            ft_main_obj ft_struct[], int mode);

int main(int argc, char const* argv[]){
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    //CLIENT SELECT IO MULTIPLEXING VARS - CREDITS: UNIX NETWORK PROGRAMMING, Stephens,
    //  Fenner, Rudoff. Vol 1 3rd Edition P178
    //-------------------------------------------------------------------------
    //INIT VARS for client ftp basic commands, plus misc vars
    fd_set allset, rset;
    int maxfd , maxi , nready , i ;//we'll
    int listen_sockfd;//main listen()ing socket for ftp clients first connections
    int status;// generic var to hold return val eg for error checking
    int client_sockfd; //once ftclient accept(), hold sock val temporarily
    struct timeval tv ;
    client_obj clients[FD_SETSIZE] ;//all accept()ed client socks stored here
    FD_ZERO(&allset);
    FD_ZERO(&rset);
    //since we are using listen_sockfd to listen(), to check if there's a new
    //connection, we place it in the read_fds set
    if((listen_sockfd = get_listensockfd(_PORT)) < 0){
        perror("setup listen_sockfd failed\n");
        exit(1);
    }
    FD_SET(listen_sockfd, &allset);
    //so far, maxfd largest file descriptor encountered
    //maxfd+1, 1st arg for select()
    maxfd = listen_sockfd;
    //maxi will hold the highest index in the clientsz array that's currently in use
    //it's set to -1, since none of the socket fds are ready
    maxi = -1;
    for(i = 0; i < FD_SETSIZE; ++i){
        reset_client_obj( &(clients[i]));
    }

    //-------------------------------------------------------------------------
    //INIT VARS for temporarily storing address vals for newly connected client
    //for logging, debugging
    struct sockaddr_in client_address ;
    int client_addrlen = sizeof(client_address) ;

    //-------------------------------------------------------------------------
    //INIT VARS for send and receive cmds, etc
    char buffer[BUF_SIZE] = {0};
    int ret = 0;//for parsing cmds
    //-------------------------------------------------------------------------
    //INIT VARS for Actual file transfer
    fd_set main_wset,wset;
    FD_ZERO(&wset);
    FD_ZERO(&main_wset);
    int ft_FTReady_sockfd, k;
    //-------------------------------------------------------------------------

    //INIT listen file transfer connection setup VARS
    fd_set ft_allset_listen, ft_rset_listen;

    int ft_client_sockfd_listen, ft_maxfd_listen, ft_maxi, ft_nready_listen, j, new_dataSOCK;//for setting up file transfer sockets
    ft_main_obj ft_struct[FD_SETSIZE];

    FD_ZERO(&ft_allset_listen);
    FD_ZERO(&ft_rset_listen);

    ft_maxfd_listen = 0;
    ft_maxi = -1;


    for(j = 0; j < FD_SETSIZE; ++j){
        reset_ft_struct_obj(&ft_struct[j]);
    }
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //INIT Vars for test ftp users
    Account Accounts_arr[5];
    init_test_accounts(Accounts_arr);
    //-------------------------------------------------------------------------
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    // FTP MAIN LOOP HERE
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    while (1){
        //-------------------------------------------------------------------------------
        rset = allset; //copy over
        wset = main_wset;
        //select returns total no of fds ready
        tv.tv_sec = 2;

        nready = select(maxfd + 1, &rset, &wset, NULL, &tv);

        if(nready == -1){perror("Select error\n"); exit(4);}//basic error checking select
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
        //         CHECK IF THERE IS A NEW CLIENT CONNECTION, ACCEPT
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
        if(FD_ISSET(listen_sockfd, &rset)){
            if((client_sockfd = accept(listen_sockfd,
                                       (struct sockaddr *)&client_address,
                                       (socklen_t*)&client_addrlen)) < 0){
                perror("accept failed");
            }else{//Accept succesful at this point, %hd, short int 2 bytes
                fprintf (stderr, "Server: accept() successful connection from HOST: %s, PORT %hu.\n",
                         inet_ntoa (client_address.sin_addr),
                         ntohs (client_address.sin_port));
                //save descriptor
                for(i = 0; i < FD_SETSIZE; ++i){
                    if(clients[i].sock < 0){
                        clients[i].sock = client_sockfd; break;}
                }//check if too many clients are connecting
                //add client_sockfd to master set
                FD_SET(client_sockfd, &allset);
                //update maxfd, note it isnt guaranteed than new client sockfd
                //will be the largest yet,
                if(client_sockfd > maxfd) maxfd = client_sockfd;
                if(i > maxi) maxi = i;
            }
            --nready;
            //if(--nready <= 0){printf("CONTINUE MR ANDERSON\n");continue; }//no more readable fds
        }
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
        //                    CHECK ALL CLIENTS FOR CMD DATA
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
        for(i = 0; i <= maxi; ++i){
            if((client_sockfd = (clients[i].sock)) < 0) continue;
            if(FD_ISSET(client_sockfd, &rset)){
                RESET_BUF(buffer);
                if((status = read(client_sockfd, buffer, BUF_SIZE)) == 0){
                    //connection closed by client
                    //TODO reset account details
                    fprintf(stderr, "%s: %d\n", "connection closed by client", client_sockfd);
                    reset_account_auth(clients[i].user_p);
                    reset_client_obj(&(clients[i]));
                    close(client_sockfd);
                    FD_CLR(client_sockfd, &allset);
                }
                else{
                    char cmd[10];
                    char arg[30] = "\0";
                    char arg2[30] = "\0";
                    char* reply;
                    ret = sscanf(buffer, "%s %s %s", cmd, arg, arg2);
                    if(ret <= 0) {
                        reply = "Invalid command 1";
                        send(client_sockfd, reply, strlen(reply), 0);
                        continue;
                    }
                    else if(strcmp("USER", cmd) == 0) {
                        user_cmd(arg, Accounts_arr, &(clients[i]));
                        continue;
                    }
                    if(strcmp("PASS", cmd) == 0){
                        pass_cmd(arg, &(clients[i]));
                    }
                    else if(clients[i].authenticated ==  0){
                        reply = "530";
                        send(client_sockfd, reply, strlen(reply), 0);
                        continue;
                    }
                    //At this point onwards, it must be the case that client is authenticated fully
                    else if(strcmp("PWD", cmd) == 0){
                        pwd_cmd(&(clients[i]));
                        continue;
                    }
                    else if(strcmp("LS", cmd) == 0){
                        list_files_cmd(&(clients[i]));
                        continue;
                    }
                    else if(strcmp("PUT", cmd) == 0){
                        //PUT cmd comes with 2 args, the filename and filesize
                        int  new_file_port = 0;
                        intmax_t filesize;
                        char lreply[10];

                        sscanf(arg2, "%jd", &filesize);
                        int fileFD;//TODO: check that we can indeed create a new file with given filesize

                        //send client PORT for connecting
                        new_file_port =  setup_ft_listening_sock(&(clients[i]), &ft_allset_listen,
                                                                 &ft_maxfd_listen, &ft_maxi,ft_struct, 2);
                        if(new_file_port < 0){
                            strcpy(lreply, "0");
                        }else{
                            sprintf(lreply, "%d",new_file_port);
                        }
                        fprintf(stdout, "PUT FILE_SIZE: %jd PORT: %d\n", filesize, new_file_port);
                        send(clients[i].sock, lreply, strlen(lreply), 0);
                        continue;

                    }
                    else if(strcmp("GET", cmd) == 0){
                        struct stat file_stat;
                        int fileFD, new_file_port = 0;
                        char lreply[200];
                        fileFD = open(arg, O_RDONLY, 0);
                        if((fstat(fileFD, &file_stat)) < 0){
                            reply = "filename: no such file on server";
                            send(clients[i].sock, reply, strlen(reply), 0);
                            continue;
                        }
                        new_file_port =  setup_ft_listening_sock(&(clients[i]), &ft_allset_listen,
                                                                 &ft_maxfd_listen, &ft_maxi,ft_struct, 1);
                        if(new_file_port < 0){
                            strcpy(lreply, "0 0");
                        }else{
                            sprintf(lreply, "%jd %d", (intmax_t) file_stat.st_size, new_file_port);
                        }
                        fprintf(stdout, "GET FILE_SIZE: %jd PORT: %d\n", (intmax_t) file_stat.st_size, new_file_port);
                        send(clients[i].sock, lreply, strlen(lreply), 0);
                        continue;
                    }
                    else{
                        reply = "Invalid command 2";
                        send(client_sockfd, reply, strlen(reply), 0);
                    }
                }
                if(--nready <= 0) {break;} //no more cmdfds
            }

        }
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
        //HERE'S WHERE WE DO ACTUAL FILE TRANSFERS, EVERY SOCKET HERE IS READY FOR FILE TRANSFER
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
        for(k = 0; k <= ft_maxi; ++k){
            if ((ft_FTReady_sockfd  = (ft_struct[k].fsock)) < 0) continue;
            if(FD_ISSET(ft_FTReady_sockfd, &wset)){
                //switch between PUT and GET
                if(ft_struct[k].mode == 1){ //GET MODE
                    //TODO THIS IS WHERE WE TRANSFER FILE TO CLIENT
                    char* freply;
                    freply = "WUBBA LUBBA DUB DUB";
                    send(ft_FTReady_sockfd, freply, strlen(freply), 0);
                }
                else if(ft_struct[k].mode == 2){//PUT MODE
                    char in_buff[BUF_SIZE];
                    read(ft_FTReady_sockfd, in_buff, BUF_SIZE);
                    printf("FROM CLIENT: %s\n", in_buff);
                }//else if == 0, error
                //clean up
                FD_CLR(ft_FTReady_sockfd, &main_wset);//, do we really need to do this????
                close(ft_FTReady_sockfd);
                reset_ft_struct_obj(&(ft_struct[k]));
                if(--nready <= 0) {break;}
            }

        }
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
        //THIS SECTION DEALS WITH SETTING UP TCP CONNECTION  & PREPARATION FOR FILE TRANSFER
        //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
        ft_rset_listen = ft_allset_listen;
        tv.tv_usec = 100000; //0.1 seconds

        ft_nready_listen = select(ft_maxfd_listen + 1, &ft_rset_listen, NULL, NULL, &tv);

        if(ft_nready_listen == -1){perror("FT Select error\n"); exit(4);}

        if(ft_nready_listen == 0) continue;//start at top of main while(1) loop

        for(j = 0; j <=ft_maxi; ++j){
            if((ft_client_sockfd_listen = ft_struct[j].lsock) <0) continue;
            if(FD_ISSET(ft_client_sockfd_listen, &ft_rset_listen)){
                //TODO: get associated user account
                //accept connection
                new_dataSOCK = accept(ft_client_sockfd_listen,
                                      (struct sockaddr *)&client_address,
                                      (socklen_t*)&client_addrlen);

                if(new_dataSOCK < -1) perror("FT accept failed");//Maybe inform FTP client??? ¯\_(ツ)_/¯
                else{
                    //TODO: WARNING CHECK THAT CLIENT IS THE ONE THAT OUGHT TO BE
                    // ACCEPTED AND NOT A 3RD PARTY
                    //REJECT IF 3RD PARTY

                    fprintf (stderr, "FTServer: FT accept() successful connection from HOST: %s, PORT %hu.\n",
                             inet_ntoa (client_address.sin_addr),
                             ntohs (client_address.sin_port));
                    //socket from accept() added to user file_transport
                    //add new_dataSOCK to writefds, now that it's set up to write files
                    if (new_dataSOCK > maxfd) maxfd = new_dataSOCK;
                    FD_SET(new_dataSOCK, &main_wset);
                    ft_struct[j].lsock = -1;
                    ft_struct[j].fsock = new_dataSOCK;
                    //the vars here are for looping through wset,to quicken or sth like tht
                }
                //remove ft_client_sockfd_listen from ft_rset_listen
                //close ft_client_sock fd
                FD_CLR(ft_client_sockfd_listen, &ft_allset_listen);
                close(ft_client_sockfd_listen);
                if(--ft_nready_listen <= 0) {break;}
            }
        }

    }//END OF WHILE LOOP
    return 0;
}


int get_listensockfd(char* PORT){
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    //CREDITS: Beej, CSAPP for the following section, on setting up our socket and binding
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    //SERVER SIDE SETUP for listen_sockfd
    int listen_sockfd;
    struct addrinfo hints, *servinfo, *p;
    // for setsockopt SO_REUSEADDR, below. In case a bit of a socket was connected and
    //it's still hogging the port, this will allow for reuse of PORT
    int opt = 1;
    int status;
    //hints will be used to set the
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;    //Use TCP
    hints.ai_flags = AI_PASSIVE;     // bind to the IP of the host it's running on -Beej
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }
    //get list of potential server addresses
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((listen_sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) < 0) {
            perror("socket failed\n");
            continue;
        }
        //SOL_SOCKET specifies the level at which the protocol resides
        //SO_REUSEADDR allows for reuse of port, incase prev socket is hogging the port
        //opt,says yes
        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(opt)) < 0) {
            perror("Error setsockopt()");
            return -1;
        }
        //forcefully attaching socket to address and PORT
        //2nd arg contains info on PORT and IP Address , of type struct sockaddr
        if (bind(listen_sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(listen_sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);//clean up
    if(p == NULL){
        fprintf(stderr, "Server failed to bind\n");
        return -1;
    }
    //listen, puts the server socket in passive mode
    if(listen(listen_sockfd, LISTEN_QUEUE_SIZE) < 0){
        perror("listen failed\n");
        return -1;
    }
    //-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    return listen_sockfd;
}

int get_port_from_sock(int sock){
    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    if(getsockname(sock, (struct sockaddr *)&sin, &addrlen) == 0 &&
       sin.sin_family == AF_INET &&
       addrlen == sizeof(sin)){
        return ntohs(sin.sin_port);
    }
    return -1;
}



void user_cmd(char* name_arg, Account Accounts_arr[], client_obj* client){
    Account* curr_account = get_account_given_usrname(Accounts_arr, name_arg);
    char* reply;
    if(curr_account == NULL) {
        reply = "Account does not exist";
        send(client->sock, reply, strlen(reply), 0); return;
    }else if(curr_account->is_sessionactive){
        reply = "User already active in other host";
        send(client->sock, reply, strlen(reply), 0); return;
    }
    else{//at this point user name is valid and it's the only active
        //session for given user, therefore set it up
        //"attach" socket/client to user in dbase
        client->user_p = curr_account;
        client->name = name_arg;
        client->pass = curr_account->pass;
        reply = "Username OK, password required";
        send(client->sock, reply, strlen(reply), 0);
    }
}




void pass_cmd(char* pass_arg, client_obj* client){
    char* reply;
    if(client->authenticated || ((client->user_p) == NULL)){
        reply = "Out of order Pass Command";
        send(client->sock, reply,strlen(reply), 0); return;
    }
    if((client->user_p)->is_sessionactive){
        reply = "User already active in other host";
        send(client->sock, reply, strlen(reply), 0); return;
    }
    if (strcmp(pass_arg, client->pass) == 0){
        reply = "Password Approved";
        client->authenticated = 1;
        (client->user_p)->is_sessionactive = 1;
    }
    else reply = "Wrong Password";
    send(client->sock, reply, strlen(reply), 0);
}

void pwd_cmd(client_obj* client){
    assert((client->user_p)!= NULL);
    char* reply = (client->user_p)->cwd;
    send(client->sock, reply, strlen(reply), 0);
}


void list_files_cmd(client_obj* client){
    assert((client->user_p)!= NULL);
    DIR *dir;
    char lreply[2048] = "\n";//Hopefully this'll be enough :-)
    char* reply;
    struct dirent *ent;
    if ((dir = opendir ((client->user_p)->cwd)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            strcat((strcat(lreply, ent->d_name)), "\n");
        }
        send(client->sock, lreply, strlen(lreply), 0);
        closedir (dir);
    }else{
        reply = "LS error";
        send(client->sock, reply, strlen(reply), 0);
        perror ("could not open directory");
    }
}
//FILE TRANSFER FUNCTIONS

//returns port for file transfer on successful completion, else returns 0
int setup_ft_listening_sock(client_obj* client, fd_set* ft_allset_listen_p,
                            int* ft_maxfd_listen_p, int* ft_maxi,
                            ft_main_obj ft_struct[], int mode)
{
    int j, new_file_listensock, new_file_port = 0;

    if((new_file_listensock = get_listensockfd("0")) < 0){
        return -1;
    }
    if((new_file_port = get_port_from_sock(new_file_listensock)) < 0){
        return -1;
    }

    for(j = 0; j < FD_SETSIZE; ++j){

        if(ft_struct[j].lsock < 0){
            ft_struct[j].lsock = new_file_listensock ;
            ft_struct[j].fsock = -1;//shall be updated once accept()
            ft_struct[j].mode = mode;//PUT sock or GET sock
            ft_struct[j].user_p = client->user_p;
            break;
        }
    }

    FD_SET(new_file_listensock, ft_allset_listen_p);

    if(new_file_listensock  > (*ft_maxfd_listen_p)) (*ft_maxfd_listen_p) = new_file_listensock;

    if(j > (*ft_maxi)) (*ft_maxi) = j;

    return new_file_port;
}
