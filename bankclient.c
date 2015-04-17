#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<pthread.h>

// Miniature client to exercise getaddrinfo(2).

int createCommThreads(int socketdescriptor);

int
connect_to_server( const char * server, const char * port )
{
	int			sd;
	struct addrinfo		addrinfo;
	struct addrinfo *	result;
	char			message[256];

	addrinfo.ai_flags = 0;
	addrinfo.ai_family = AF_INET;		// IPv4 only
	addrinfo.ai_socktype = SOCK_STREAM;	// Want TCP/IP
	addrinfo.ai_protocol = 0;		// Any protocol
	addrinfo.ai_addrlen = 0;
	addrinfo.ai_addr = NULL;
	addrinfo.ai_canonname = NULL;
	addrinfo.ai_next = NULL;
	if ( getaddrinfo( server, port, &addrinfo, &result ) != 0 )
	{
		fprintf( stderr, "\x1b[1;31mgetaddrinfo( %s ) failed.  File %s line %d.\x1b[0m\n", server, __FILE__, __LINE__ );
		return -1;
	}
	else if ( errno = 0, (sd = socket( result->ai_family, result->ai_socktype, result->ai_protocol )) == -1 )
	{
		freeaddrinfo( result );
		return -1;
	}
	else
	{
		do {
			if ( errno = 0, connect( sd, result->ai_addr, result->ai_addrlen ) == -1 )
			{
				sleep( 1 );
				write( 1, message, sprintf( message, "\x1b[2;33mConnecting to server %s ...\x1b[0m\n", server ) );
			}
			else
			{
				freeaddrinfo( result );
				return sd;		// connect() succeeded
			}
		} while ( errno == ECONNREFUSED );
		freeaddrinfo( result );
		return -1;
	}
}

void *
command_input_thread( void * arg ) //This thread 
{
	
	int			sd;
	char		input[512];
	char		response[2048];
	char		prompt[] = "Enter your command>>";
	int size;

	sd = *(int *)arg;
	free( arg );					// keeping to memory management covenant
		
	while (write( 1, prompt, sizeof(prompt) ), read(0, input, sizeof(input)) > 0 )
	{
		size = strlen(input);
		input[size-1]= '\0';
		write(sd, input, strlen(input) + 1 );
		sleep(2);
	}

	close( sd );	
	return 0;
}

void *
response_output_thread( void * arg )
{
	
	int			sd;
	char		request[2048];
	char		response[2048];
	char		temp;
	int			i;
	int			limit, size;
	float		ignore;
	long		senderIPaddr;

	sd = *(int *)arg;
		// keeping to memory management covenant
		// Don't join on this thread
	while ( read( sd, request, sizeof(request) ) > 0 )
	{
		write( 1, request, strlen(request) );
	}
	close( sd ); 

	return 0;
	
}
int
main( int argc, char ** argv )
{
	int			sd;
	char			message[256];
	char			string[512];
	char			buffer[512];
	char			prompt[] = "Enter a string>>";
	char			output[512];
	int			len;
	int			count;

	if ( argc < 2 )
	{
		fprintf( stderr, "\x1b[1;31mNo host name specified.  File %s line %d.\x1b[0m\n", __FILE__, __LINE__ );
		exit( 1 );
	}
	else if ( (sd = connect_to_server( argv[1], "3000" )) == -1 )
	{
		write( 1, message, sprintf( message,  "\x1b[1;31mCould not connect to server %s errno %s\x1b[0m\n", argv[1], strerror( errno ) ) );
		return 1;
	}
	else
	{

		printf( "Connected to server %s\n", argv[1] );
		createCommThreads(sd);
		

		/*
		while ( write( 1, prompt, sizeof(prompt) ), (len = read( 0, string, sizeof(string) )) > 0 )
		{
			
			string[len-1]= '\0';
			write( sd, string, strlen( string ) + 1 );
			

			write(sd, string, 1);
			read(sd, buffer, sizeof(buffer) );
			sprintf( output, "Result is >%s<\n", buffer );
			write( 1, output, strlen(output) );
		}
		*/
		close( sd );
		return 0;
	}
}

int createCommThreads(int socketdescriptor){ //this method creates the response_output_thread and command_input thread
	pthread_t		tid;
	pthread_attr_t	kernel_attr;

	pthread_t		tid2;
	pthread_attr_t	kernel_attr2;

	int * sdptr = (int *)malloc( sizeof(int *) );
	*sdptr = socketdescriptor;

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
	if ( pthread_create( &tid, &kernel_attr, response_output_thread, sdptr) != 0 )
		{
			printf( "pthread_create() failed in file %s line %d\n", __FILE__, __LINE__ );
			return 0;
		}	

	if ( pthread_attr_init( &kernel_attr2 ) != 0 )
		{
			printf( "pthread_attr_init() failed in file %s line %d\n", __FILE__, __LINE__ );
			return 0;
		}
	else if ( pthread_attr_setscope( &kernel_attr2, PTHREAD_SCOPE_SYSTEM ) != 0 )
		{
		printf( "pthread_attr_setscope() failed in file %s line %d\n", __FILE__, __LINE__ );
		return 0;
	}
	if ( pthread_create( &tid, &kernel_attr, command_input_thread, sdptr ) != 0 )
	{
		printf( "pthread_create() failed in file %s line %d\n", __FILE__, __LINE__ );
		return 0;
	}
	
	pthread_join(tid,NULL);
	//pthread_join(tid2,NULL);

	return 1;
}