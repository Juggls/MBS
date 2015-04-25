#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<pthread.h>
#include        <signal.h>
#include        <sys/shm.h>
#include        <sys/wait.h>
#include        <semaphore.h>
#include        "server.h"
#include        <sys/time.h>

#define QUIT 7
#define END 1
#define QUERY 2
#define CREATE 3
#define SERVE 4
#define DEPOSIT 5
#define WITHDRAW_SUCCESS 10
#define WITHDRAW_OVERFLOW 11


static int glob_shmid;
static Bank * glob_shmaddr;
static int glob_sd;
static char curr_account[100];
static int inservice = 0; 
static int index = 0;


void printBankinfo(){ //attempts to lock the bank. If successful, prints out information. Happens everytime our alarm goes off. 
    int i, z;     
 
    sem_wait(&glob_shmaddr->lock);
    if(glob_shmaddr->num_accounts == 0){
        write(1, "There are no accounts currently in the bank\n", strlen("There are no accounts currently in the bank\n"));       
	sem_post(&glob_shmaddr->lock);
        return;
    } 
    char message[1500];
    memset(message, 0, 1500);

    char  imessage[20][150]; 
    char * is;
  
    strcat(message, "Time expired, here is a list of bank information:\n");
    for(i = 0; i < glob_shmaddr->num_accounts; i++){
        if(glob_shmaddr->accounts[i].flag == 0){
            is = "false";
        }
	else{
	    is = "true";
	}
        sprintf(imessage[i], "Account %d: Name = %s, Balance = %f, In Session = %s\n",i, glob_shmaddr->accounts[i].name,glob_shmaddr-> accounts[i].balance, is);
    }

    for(z = 0; z < glob_shmaddr->num_accounts; z ++){
        strcat(message, imessage[z]);
    }
    
    write(1, message, strlen(message) + 1);
   
    sem_post(&glob_shmaddr->lock);
    return;

}

void timeout_handler( int sig) /*blocks signal to ensure that no more than one timer interrupt comes, then resets the timers*/
{
    sigset_t blockalrm;
    sigemptyset (&blockalrm);
    sigaddset (&blockalrm, SIGALRM);
    sigprocmask (SIG_BLOCK, &blockalrm, NULL); //block more alarm signals from coming in
    printBankinfo(); 
    sigprocmask (SIG_UNBLOCK, &blockalrm, NULL);//unblock alarm signals 
    alarm(20);

}

void create_shm() //creates our shared memory
{
    key_t key;
    int shmid;
    
    key = ftok("/server", 'a');

    if ((shmid = shmget(key, sizeof(Bank), IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(1);
    }
    
    glob_shmid = shmid;

    if (*(int*)(glob_shmaddr = shmat(shmid, NULL, 0)) == -1)
    {
        perror("shmat");
        exit(1);
    }

    sem_init(&glob_shmaddr->lock, 1, 1);
    glob_shmaddr->num_accounts = 0;
}

void set_shm() //sets our shared memory up
{
    key_t key;
    int shmid;

    key = ftok("/server", 'a');

    if ((shmid = shmget(key, sizeof(Bank), 0666)) < 0)
    {
        perror("shmget");
        exit(1);
    }

    if (*(int*)(glob_shmaddr = shmat(shmid, NULL, 0)) == -1)
    {
        perror("shmat");
        exit(1);
    }
}
/*Creates our account. First has to lock down the bank semaphore. Then it there is max capacity or the account exists it returns without creation. If less than capacity and doesn't exist yet, intializate the account with a semaphore*/
int create_account(char * account_name,int  fd){
    char message[256];
    memset(message,0,256);    
    sem_wait(&glob_shmaddr->lock);
    write(fd, message, sprintf(message, "Unlocked the bank semaphore for account creation\n"));
     
    int num = glob_shmaddr->num_accounts;
    if(num == 20){ //creation fails
	sem_post(&glob_shmaddr->lock); //give up your lock
	write(fd,message, sprintf(message,"\tYour Bank is full, no more accounts can be created\n"));
        return -1; 
    }

    for(int i = 0; i < num; i ++){
        if(strcmp(account_name, glob_shmaddr->accounts[i].name) == 0){
            sem_post(&glob_shmaddr->lock);
            write(fd, message, sprintf(message, "\tBank account with this name already exists\n"));
            return -1; 
        }
    }

    sem_init(&glob_shmaddr->accounts[num].lock, 1, 1);
    strcpy(glob_shmaddr->accounts[num].name, account_name);
    glob_shmaddr->accounts[num].name[strlen(account_name)] = '\0';
    glob_shmaddr->accounts[num].balance = 0;
    glob_shmaddr->accounts[num].flag = 0;
    glob_shmaddr->num_accounts++;

    sem_post(&glob_shmaddr->lock);
    
    
    return CREATE;
}

int query(char * response)
{
    char message[256];
    memset(message, 0, 256);
    if (inservice == 0) 
        return -1;

    sprintf(message, "Current balance is %.2f\n", glob_shmaddr->accounts[index].balance);
    strcpy(response, message);
    message[strlen(message)] = '\0';
    return QUERY;
}

int deposit(char * amount){
    int dep_amt;
    if(!inservice){
        return -1;
    }

    if ((dep_amt = atof(amount)) == 0)
        return -1;
    
    glob_shmaddr->accounts[index].balance = glob_shmaddr->accounts[index].balance + dep_amt;
    return DEPOSIT;  //success
}
/*Withdraws and ammount, can only execute if in serving session*/
int withdraw(char * amount){ 
    int with_amt = 0;
    if(!inservice){
        return -1;
    }

    if((with_amt = atof(amount)) == 0)
        return -1;

    if(with_amt > glob_shmaddr->accounts[index].balance){ //if you are trying tow ithdraw more than possible
        return WITHDRAW_OVERFLOW;
    }

    glob_shmaddr->accounts[index].balance = glob_shmaddr->accounts[index].balance - with_amt;

    return WITHDRAW_SUCCESS; //success
}

/*ends our serving session. Resets flags and frees up the semaphore 
 */
int end(){ 
    if(!inservice){ //if you aren't currently in service
        return -1; 
    }
    glob_shmaddr->accounts[index].flag = 0; 
    inservice = 0;
    memset(curr_account, 0, sizeof(curr_account));
    sem_post(&glob_shmaddr->accounts[index].lock); //release mutex
    index = 0;
    return END; 
}
/*tries to enter serving session with account. If it cannot enter the serving session it will wait and tell the client that it is waitng. Cannot enter session if you are currently in a session, or the account doesn't exist*/
int serve_account(char * account, int fd){
    if (strcmp(account, curr_account) == 0){
        return -1;
    }
    if(inservice == 1){
	return -1; 
    }

    sem_wait(&glob_shmaddr->lock); //wait on the bank lock
    //printf("Bank semaphore locked for account %s\n", account);
    int i;
    if(glob_shmaddr->num_accounts == 0){
        write(fd, "No accounts currently exist, so you cannot serve\n", strlen("No accounts currently exist, so you cannot serve\n")+ 1);
        sem_post(&glob_shmaddr->lock);
        return -1; 
    } 
    for(i = 0; i < glob_shmaddr->num_accounts; i++){
        if(strcmp(account, glob_shmaddr->accounts[i].name) == 0){
            //sem_getvalue(&glob_shmaddr->accounts[i].lock, &sem_val);
            char message[200];
            memset(message,0,200);

            sprintf(message, "Customer account %s is currently in a service session\n", glob_shmaddr->accounts[i].name);
            while(sem_trywait(&glob_shmaddr->accounts[i].lock)!=0){ //let's get some xc! 
		sem_post(&glob_shmaddr->lock);//give up lock so server can continue processing	
		write(fd, message, strlen(message));
                sleep(3);
		sem_wait(&glob_shmaddr->lock);//get back lock and try again  			
	    }			 
	   
            
            glob_shmaddr->accounts[i].flag = 1;;
            strcpy(curr_account, account);
            curr_account[strlen(account)] = '\0';
            inservice = 1;
            index = i;
            break; 
        }
    }
    if(i == glob_shmaddr->num_accounts){
	write(fd, "You didn't enter a valid account name, dummy\n", strlen("You didn't enter a valid account name, dummy\n"));
	sem_post(&glob_shmaddr->lock);;
	return -1; 
    }   
    sem_post(&glob_shmaddr->lock);
    
    return SERVE; 
}

/*parses our input, updates the client and calls method */
int parse_request(char * request, char * response, int fd)
{
    char parser[150];
    char split[2] = " ";
    char parameter[150];
    char *token;
    int i = 0;
    int request_code;


    if (strcmp(request, "quit") == 0)
        request_code = QUIT;
    else if (strcmp(request, "end") == 0)
        request_code = END;
    else if (strcmp(request, "query") == 0)
        request_code = QUERY;
    else {
        strcpy(parser, request);
        parser[strlen(request)] = '\0';
        
        token = strtok(parser, split);
        
        if (strcmp(parser, "create") == 0) {
            request_code = CREATE;
        } else if (strcmp(parser, "serve") == 0) {
            request_code = SERVE;
        } else if (strcmp(parser, "deposit") == 0) {
            request_code = DEPOSIT;
        } else if (strcmp(parser, "withdraw") == 0) {
            request_code = WITHDRAW_SUCCESS;
        } else {
            return -5;
        }

        while (token != NULL) {
            i++;
            if (i == 2) {
                strcpy(parameter, token);
                parameter[strlen(token)] = '\0';
            }
            token = strtok(NULL, split);
        }

        if (i > 2 || i <= 1)
            return -5;
    }

    char message[250];
    memset(message, 0, 250);
    switch(request_code)
    {
        case CREATE:
            write(1, message, sprintf(message,"\x1b[2;33mAttempting to create new account \"%s\"\x1b[0m\n", parameter));
            request_code = create_account(parameter, fd);
            break;
        case SERVE:
            write(1, message,sprintf(message, "\x1b[2;33Attempting to serve account \"%s\"\x1b[0m\n", parameter));
            request_code = serve_account(parameter, fd);
            break;
        case QUERY:
            write(1,message, sprintf(message,"\x1b[2;33mAttempting to query account \"%s\"\x1b[0m\n", curr_account));
            request_code = query(response);
            break;
        case DEPOSIT:
            request_code = deposit(parameter);
            break;
        case WITHDRAW_SUCCESS:
            request_code = withdraw(parameter);
            break;
        case END:
            request_code = end();
            break;
    }

    return request_code; 
}


int client_session_actions(int sd)
{
    char request[512];
    char response[512];
    char message[256];
    int request_code;
    memset(response, 0, 512);
    glob_sd = sd;
    
    while ( read( sd, request, sizeof(request) ) > 0 )
    {
        printf( "==>Server receives input:  %s\n", request );

        if ((request_code = parse_request(request, response, sd)) == -5) {
            write(sd, response, sprintf(response, "\x1b[1;31mFaulty input: please try again\x1b[0m\n"));
        } else {
            switch(request_code)
            {
                case QUERY:                
                    write( sd, response, strlen(response) + 1 );
                    break;
                case WITHDRAW_SUCCESS:    
                    write(sd, response, sprintf(response, "Withdrawal succeeded!\n"));
                    break;
                case WITHDRAW_OVERFLOW:
                    write(sd, response, sprintf(response, "Withdrawl overflow - balance too low, try again\n"));
                    break;
                case -1:
                    write(sd, response, sprintf(response, "Operation request \"%s\" failed\n", request)); 
                    break;
                default:
                    write(sd, response, sprintf(response, "Operation request \"%s\" succeeded!\n", request));
            }
        }
        
        if (request_code == QUIT) {
	    if(inservice){ //potentially needs to release a semaphore
		sem_post(&glob_shmaddr->accounts[index].lock);
		glob_shmaddr->accounts[index].flag = 0;
	    }
            write(1, message, sprintf(message, "Quitting child process %d\n", getpid()));
            exit(0);
        }
    }
    close( sd );
    return 0;
}


/*
 * reap up those juicy zombies
 */
void sigchld_handler(int s)
{
    int pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Killing child process %d\n", pid);
    }
}

/*
 * Signal handler for SIGINT
 */
void sigint_handler(int sig)
{   
    if (shmdt(glob_shmaddr) != 0) { //clear up all shared memory
        perror("shmdt");
        exit(1);
    }

    shmctl(glob_shmid, IPC_RMID, NULL);
    write(glob_sd, "disconnect", 11); //kill client process
    exit(0);
}

int main( int argc, char ** argv )
{
    int sd;
    char message[256];
    pthread_attr_t kernel_attr;
    socklen_t ic;
    int	fd;
    struct sockaddr_in senderAddr;
    pid_t child_pid;

    struct sigaction sigint_action;
    struct sigaction sigchld_action;
    struct sigaction sigalrm_action;

    sigalrm_action.sa_handler = timeout_handler;
    sigalrm_action.sa_flags = SA_RESTART; 
    sigaction(SIGALRM, &sigalrm_action, NULL);;
    alarm(3);

    sigint_action.sa_handler = sigint_handler;
    sigemptyset(&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;

    if (sigaction(SIGINT, &sigint_action, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    sigchld_action.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sigchld_action.sa_mask);
    sigchld_action.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sigchld_action, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
   ;
   
 
    if ( pthread_attr_init( &kernel_attr ) != 0 )
    {
        printf( "pthread_attr_init() failed in file %s line %d\n", __FILE__, __LINE__ );
        return 0;
    }
    else if ( pthread_attr_setscope( &kernel_attr, PTHREAD_SCOPE_SYSTEM ) != 0 )
    {
        printf( "pthread_attr_setscope() failed in file %s line %d\n", __FILE__, __LINE__ );
        return 0;
    }
    else if ( (sd = claim_port( "51628" )) == -1 )
    {
        write( 1, message, sprintf( message,  "\x1b[1;31mCould not bind to port %s errno %s\x1b[0m\n", "51268", strerror( errno ) ) );
        return 1;
    }
    else if ( listen( sd, 100 ) == -1 )
    {
        printf( "listen() failed in file %s line %d\n", __FILE__, __LINE__ );
        close( sd );
        return 0;
    }
    else if (create_shm(), glob_shmaddr == NULL) //creating shared memory
    {
        printf("NULL shared mem returned when creating\n");
        return 0;
    }
    else
    {
        write(1, message, sprintf(message, "\x1b[1;32mShared memory created successfully\x1b[0m\n")); 
        ic = sizeof(senderAddr);
        while ( (fd = accept( sd, (struct sockaddr *)&senderAddr, &ic )) != -1 )
        {
            write(1, message, sprintf(message, "\x1b[1;32mClient accepted at file descriptor %d\x1b[0m\n", fd));
            child_pid = fork();
            if (child_pid >= 0) {
                if (child_pid == 0) //know this is child processes
                {
		    sigalrm_action.sa_handler = SIG_IGN; //ignore timer calls in child process
		    close(sd);
                    client_session_actions(fd);
                    close(fd);
                    
                }
                else //this is the parent process
                {
                    write(1, message, sprintf(message, "\x1b[1;32mCreated child process %d\x1b[0m\n", child_pid));
                    close(fd);
                }
            }
        }
        close( sd );
        return 0;
    }
}

int claim_port( const char * port )
{
    struct addrinfo addrinfo;
    struct addrinfo *	result;
    int	sd;
    char message[256];
    int on = 1;

    addrinfo.ai_flags = AI_PASSIVE;		// for bind()
    addrinfo.ai_family = AF_INET;		// IPv4 only
    addrinfo.ai_socktype = SOCK_STREAM;	// Want TCP/IP
    addrinfo.ai_protocol = 0;		// Any protocol
    addrinfo.ai_addrlen = 0;
    addrinfo.ai_addr = NULL;
    addrinfo.ai_canonname = NULL;
    addrinfo.ai_next = NULL;
    if ( getaddrinfo( 0, port, &addrinfo, &result ) != 0 )		// want port 3000
    {
        fprintf( stderr, "\x1b[1;31mgetaddrinfo( %s ) failed errno is %s.  File %s line %d.\x1b[0m\n", port, strerror( errno ), __FILE__, __LINE__ );
        return -1;
    }
    else if ( errno = 0, (sd = socket( result->ai_family, result->ai_socktype, result->ai_protocol )) == -1 )
    {
        write( 1, message, sprintf( message, "socket() failed.  File %s line %d.\n", __FILE__, __LINE__ ) );
        freeaddrinfo( result );
        return -1;
    }
    else if ( setsockopt( sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) ) == -1 )
    {
        write( 1, message, sprintf( message, "setsockopt() failed.  File %s line %d.\n", __FILE__, __LINE__ ) );
        freeaddrinfo( result );
        close( sd );
        return -1;
    }
    else if ( bind( sd, result->ai_addr, result->ai_addrlen ) == -1 )
    {
        freeaddrinfo( result );
        close( sd );
        write( 1, message, sprintf( message, "\x1b[2;33mBinding to port %s ...\x1b[0m\n", port ) );
        return -1;
    }
    else
    {
        write( 1, message, sprintf( message,  "\x1b[1;32mSUCCESS : Bind to port %s\x1b[0m\n", port ) );
        freeaddrinfo( result );		
        return sd;			// bind() succeeded;
    }
}

