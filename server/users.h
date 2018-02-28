#include <stdio.h>
#include <string.h>
#define no_of_accs 5
#define DIR_PATH_SIZE_U 1024
typedef struct{
    char* name;
    char* pass;
    int is_sessionactive;
    int is_authorized;
    char cwd[DIR_PATH_SIZE_U];
} Account;//Static details

char main_user_dirs[DIR_PATH_SIZE_U];

void print_account(Account* acc){
    if(acc){
        printf("Name:%s Pass:%s\n",acc->name, acc->pass);
        printf("\tcwd:\t %s\n",      acc->cwd);
        printf("\tis Auth:\t %d\n\n",acc->is_authorized);
    }
}

void add_account(Account* acc, char* _name, char* _pass, char* main_dir ){
    memset(acc, 0, sizeof(Account));
    acc->name = _name;
    acc->pass = _pass;
    acc->is_sessionactive = 0;
    acc->is_authorized = 0;
    strcat(strcat(strcpy(acc->cwd, main_dir),"/"), _name);
    if ((mkdir(acc->cwd, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) == -1){
        perror("Mdkir error user dir");
    }
    //print_account(acc);
}




void init_test_accounts(Account Accounts_arr[]){

    getcwd(main_user_dirs, DIR_PATH_SIZE_U);
    strcat((strcat(main_user_dirs, "/")), "USER_DIRS");
    int status;
    status = mkdir(main_user_dirs,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
    if (status == -1){
        perror("Mdkir error user dir");
    }
    add_account(Accounts_arr + 0, "JOY", "1234", main_user_dirs);
    add_account(Accounts_arr + 1, "FOO", "BAR", main_user_dirs);
    add_account(Accounts_arr + 2, "anonymous", "", main_user_dirs);
}



void reset_account_auth(Account* acc){
    if(acc){
        acc->is_sessionactive = 0;
        acc->is_authorized = 0;
        strcpy(acc->cwd, strcat((strcat(main_user_dirs, "/")), acc->name));
    }
}


Account* get_account_given_usrname(Account Accounts_arr[], char* name){
    for (int i = 0; i < no_of_accs; ++i){
        if(strcmp(name, Accounts_arr[i].name) == 0)
            return Accounts_arr + i;
    }
    return NULL;
}













































































//end
