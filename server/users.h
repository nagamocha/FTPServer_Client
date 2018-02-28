#include <stdio.h>
#include <string.h>
#define no_of_accs 5
typedef struct{
    char* name;
    char* pass;
    char* cwd;
    int is_sessionactive;
    int is_authorized;
} Account;//Static details

void add_account(Account* acc, char* _name, char* _pass){
    memset(acc, 0, sizeof(Account) );
    acc->name = _name;
    acc->pass = _pass;
    acc->cwd = "./";
    acc->is_sessionactive = 0;
    acc->is_authorized = 0;
}



void print_account(Account* acc){
    if(acc){
        printf("Name:%s Pass:%s\n",acc->name, acc->pass);
        printf("\tcwd:\t %s\n",      acc->cwd);
        printf("\tis Auth:\t %d\n\n",acc->is_authorized);
    }
}

void init_test_accounts(Account Accounts_arr[]){
    add_account(Accounts_arr + 0, "JOY", "1234");
    add_account(Accounts_arr + 1, "KIL", "9089");
    add_account(Accounts_arr + 2, "POR", "4567");
    add_account(Accounts_arr + 3, "SAP", "8989");
    add_account(Accounts_arr + 4, "IMM", "9930");
}



void reset_account_auth(Account* acc){
    if(acc){
        acc->is_sessionactive = 0;
        acc->is_authorized = 0;
        acc->cwd = "./";
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
