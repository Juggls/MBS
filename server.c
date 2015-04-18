#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<pthread.h>
#include    	<signal.h>
#include 	<semaphore.h>
#include	<sys/shm.h>

void sethandlers();


typedef struct Account{
	sem_t  insession; 
	int flag; //0 = not in session, 1 = insession 
	char name[100]; 
	float balance;
}Account;

typedef struct Bank{
	sem_t  inuse; 
	int numaccounts;
	char * names[20];
	Account accounts[20];
}Bank;

Bank * bank_shm; //pointer to shared memory so everyone can delete
int glob_shmid;
int glob_fd;

void sigint_handler(int sig){ //gets called by a ctrl c or quit 
	if(shmdt(bank_shm) != 0){
		perror("shmdt");
		exit(1);
	}
	shmctl(glob_shmid, IPC_RMID, NULL);
	write(glob_fd, "disconnect", 11);
	exit(0);
}

int createAccount(char * newacc){
	sem_wait(&bank_shm->inuse);
	write(1,"locked semmaphore in createaccount\n", 100);
	int i; 

	int num = bank_shm->numaccounts;
	if(num == 20){ //creation fails
		return -1; 
	}
	
	for(i = 0; i < num; i ++){
		if(strcmp(newacc, bank_shm->names[i]) == 0){
			return -1; 
		}
	}

	sem_init(&(bank_shm->accounts[num].insession), 1, 1);
	strcpy(bank_shm->accounts[num].name, newacc);
	bank_shm->accounts[num].name[strlen(newacc)] = '\0';
	bank_shm->accounts[num].balance = 0;
	bank_shm->accounts[num].flag = 0;
	bank_shm->numaccounts++;

	sem_post(&bank_shm->inuse);
	write(1,"unlocked semmaphore in createaccount\n", 100);
	return 1;

}

void printBankinfo(){
	int i, j, z;	
	sem_wait(&bank_shm->inuse);
	write(1,"Locked semmaphore in printbank\n", 100);

	char message[1000];

	char * imessage[bank_shm->numaccounts]; 
	char * is = malloc(100);
	Account * accounts = bank_shm->accounts;

	for(j = 0; j < bank_shm->numaccounts; j++){
		imessage[j] = malloc(sizeof(char) *200); //allocate messagesizee
	}

	for(i = 0; i < bank_shm->numaccounts; i++){
		
		is = (accounts[i].flag == 0) ? "false": "true";
		sprintf(imessage[i], "Account 1: Name = %s, Balance = %f, In Session = %s\n", accounts[i].name, accounts[i].balance, is);
	}

	for(z = 0; z < bank_shm->numaccounts; z ++){
		strcat(message, imessage[z]);
	}
	

	write(1, message, strlen(message) + 1);

	sem_post(&bank_shm->inuse);
	write(1,"unlocked semmaphore in printbank\n", 100);
	return;

}


void handlesigchld(int sig) {
  while (wait(NULL) != -1);
}


void clientsession(int fd){
	char request[2048];
	char response[2048];
	char message[100];


	while (read( fd, request, sizeof(request)) > 0 )
	{
		write(1, request, strlen(request));
		char c = request[0]; //get first character
		char * command[100];
		createAccount(request);
		//sleep(2);
		//printBankinfo();
		//sleep(2);

		/*
		
		switch(c){
			case 'c':
				command = strncpy(command, request, 6);
				if(strcmp(command, 'create') != 0){
					response = "invalid command";
				}

				accountname = strncpy()

				


				break;
			case 's':
				command = strncpy(command, request, 5);t
				if(strcmp(command, 'serve') != 0){
					response = "invalid command";
				}
				break;
			case 'd':
				command = strncpy(command, request, 6);
				if(strcmp(command, 'deposit') != 0){
					response = "invalid command";

				}
				break;	
			case 'w':
				command = strncpy(command, request, 8);
				if(strcmp(command, 'withdraw') != 0){
					response = "invalid command";

				}
				break;
			case 'e':
				command = strncpy(command, request, 3);
				if(strcmp(command, 'end') != 0){
					response = "invalid command";
				}
				break;	
			case 'q':
				command = strncpy(command, request, 5);
				if(strcmp(command, 'query') != 0 && strcmp(command, 'quit') != 0){
					response = "invalid command";
				}
				break;
			default:
				response = "invalid command";
		}
		*/
	}

	sprintf(message, "Child Process %d has exited", getpid());
}

int
claim_port( const char * port )
{
	struct addrinfo		addrinfo;
	struct addrinfo *	result;
	int			sd;
	char			message[256];
	int			on = 1;

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


int
main( int argc, char ** argv )
{


	int			sd;
	char			message[256];
	pthread_t		tid;
	pthread_attr_t		kernel_attr;
	socklen_t		ic;
	int			fd;
	struct sockaddr_in      senderAddr;
	int *			fdptr;
	int childPID;
	key_t key;
	int shmid;
		

	sethandlers();
	
	key = ftok("/server", 'a');

	if ((shmid = shmget(key, sizeof(Bank), IPC_CREAT | 0666)) < 0){
		perror("shmget");
		exit(1);
	}
	
	if(*(int*)(bank_shm = shmat(shmid, NULL, 0)) == -1){
		perror("shmat");
		exit(1);
	}
	
	sem_init(&(bank_shm->inuse), 1, 1); //initialize semaphore for bank and set it initally to 1

	bank_shm->numaccounts = 0; //initial num of acdunts is 0 
	

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
	else if ( (sd = claim_port( "3000" )) == -1 )
	{
		write( 1, message, sprintf( message,  "\x1b[1;31mCould not bind to port %s errno %s\x1b[0m\n", "3000", strerror( errno ) ) );
		return 1;
	}
	else if ( listen( sd, 100 ) == -1 )
	{
		printf( "listen() failed in file %s line %d\n", __FILE__, __LINE__ );
		close( sd );
		return 0;
	}
	else
	{
		ic = sizeof(senderAddr);
		while ( (fd = accept( sd, (struct sockaddr *)&senderAddr, &ic )) != -1 ) //gets communication 
		{

			write(1,"Server accepted new connection", 50); //announce completion of connection to client 
			pid_t pidi = fork();
			if(pidi>=0){
				if(pidi == 0){ //if you are in child
					char childcreate[100];
					sprintf(childcreate, "Created child process %d", getpid());
					write(1, childcreate, 100);
					glob_fd = fd;
					clientsession(fd); //spawn your client session process
				}
				else{
					close(fd);
				}
			}

		}

		close(sd);
		shmdt(bank_shm);
		sem_destroy(&bank_shm->inuse);

		return 0;
	}

}

void sethandlers(){
	struct sigaction sa;
	sa.sa_handler = &handlesigchld;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

	if (sigaction(SIGCHLD, &sa, 0) == -1) {
  		perror(0);
  		exit(1);
	}

	struct sigaction sigint_action;

	sigint_action.sa_handler = &sigint_handler;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);


}

