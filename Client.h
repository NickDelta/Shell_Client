//
// Created by delta on 3/1/20.
//

#include <stdio.h>

#ifndef SHELL_CLIENT_H
#define SHELL_CLIENT_H

//Start the client
void initiate_client(char*, unsigned short);

//Signal handlers
void sigpipe_handler(int);
void sigint_handler(int);

//Custom implementations of recv and send
ssize_t recv_all(int, char**, int);
ssize_t send_all(int, char*, ssize_t);

//Handling user input
void input_handler(char*, int, char*);

//Print the history file
void create_history();
void print_history();
void delete_history();

#endif //SHELL_CLIENT_H
