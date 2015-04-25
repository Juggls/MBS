#ifndef SERVER_H
#define SERVER_H 

#include <semaphore.h>

typedef struct Account
{
    sem_t lock;
    int flag;
    char name[100];
    float balance;
} Account;

typedef struct Bank
{
    sem_t lock;
    int num_accounts;
    Account accounts[20];
} Bank;

void create_shm();

void get_shm();

int claim_port(const char * port);

int client_session_actions(int sd);

int parse_request(char * request, char * response, int fd);

#endif


