#ifndef CLIENT_H
#define CLIENT_H

void * command_input_thread(void * arg);

void * response_output_thread(void * arg);

int connect_to_server(const char * server, const char * port);

int create_threads(int sd);

#endif

