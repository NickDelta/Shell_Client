#include "Client.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 4096

int clientSocket;
FILE* file;

#define PIN_SUCCESS 0
#define PIN_FAILURE 1

void sigpipe_handler(int signum)
{
    delete_history();
    close(clientSocket);
    puts("SIGPIPE triggered");
    exit(EXIT_FAILURE);
}

void sigint_handler(int signum)
{
    delete_history();
    close(clientSocket);
    printf("\nClient terminated\n");
    exit(EXIT_SUCCESS);
}

ssize_t recv_all(int socket, char **server_response, int flags)
{
    ssize_t received_bytes;

    //Get the total size of the transmission
    ssize_t size = 0;
    received_bytes = read(socket, &size, sizeof(size));
    //printf("Preparing to receive %ld bytes total\n",size);
    if (received_bytes <= 0)
        return received_bytes;

    /* THIS IMPLEMENTATION COULD ALSO WORK WITH A STATICALLY
     * ALLOCATED BUFFER THAT HAS SIZE EQUAL TO THE CHUNK DATA + 1.
     * THE DIFFERENCE IS THAT THE BUFFER MUST BE PRINTED AFTER
     * EACH CHUNK IS READ SUCCESSFULLY*/

    //Allocate a buffer big enough for the data to fit
    *server_response = malloc((size+1) * sizeof(char));
    bzero(*server_response,size + 1);

    ssize_t toread = size;
    ssize_t total_read = 0;

    //While there are data left to be transmitted
    while (toread > 0)
    {
        //Get chunk_size from server
        ssize_t chunk_size;
        received_bytes = read(socket, &chunk_size, sizeof(chunk_size));
        //printf("chunk_size = %ld\n",chunk_size);
        if (received_bytes <= 0)
            return received_bytes;

        //Read the whole chunk
        ssize_t chunk_read = 0;
        while(chunk_read < chunk_size)
        {
            //Receive the chunk data
            received_bytes = recv(socket, *server_response + total_read, chunk_size - chunk_read, flags);
            //printf("Read %ld bytes\n",received_bytes);
            if (received_bytes <= 0)
                return received_bytes;

            chunk_read += received_bytes;
            total_read += received_bytes;
            //printf("Total chunk read = %ld\n",chunk_read);
        }
        toread -= chunk_read;
    }
    return total_read;
}

ssize_t send_all(int socket, char *buf,ssize_t size)
{
    /* LIKE THE SERVER VERSION */

    ssize_t total_bytes = 0;        // how many bytes we've sent
    ssize_t bytes_sent;

    //printf("Notifying server that I want to send %ld bytes total \n",size);
    send(socket,&size, sizeof(size),0); //FOR CLIENT

    ssize_t chunk_size;
    while(total_bytes < size)
    {
        chunk_size = size - total_bytes;

        //printf("Notifying server that I want to send a %ld byte chunk\n",chunk_size);
        bytes_sent = send(socket, &chunk_size, sizeof(chunk_size), 0);

        if(bytes_sent <= 0)
            return bytes_sent;

        bytes_sent = send(socket, buf + total_bytes, chunk_size, 0);
        //printf("Sent %ld bytes\n",n);
        if (bytes_sent <= 0)
            return bytes_sent;

        total_bytes += bytes_sent;
    }
    return total_bytes;
}

void input_handler(char* buffer, int buf_size, char* ip_addr)
{
    //Gets user input
    bzero(buffer, sizeof(buf_size));
    printf("\n%s_>: ",ip_addr);
    int n = 0;
    while ((buffer[n++] = getchar()) != '\n');
    buffer[n - 1] = '\0';
}

void create_history()
{
    /*Filename is unique for each client instance on
     *the same machine thanks to the PID*/

    char filename[30];
    sprintf(filename,"his%d",getpid());

    //Create history file
    file = fopen(filename,"w+");
    if(file == NULL)
    {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }
}

void print_history()
{
    fseek(file,0,SEEK_SET);
    char c = fgetc(file);
    while (c != EOF)
    {
        printf ("%c", c);
        c = fgetc(file);
    }
}

void delete_history()
{
    int exit_code ;
    //Close the file if it has been created
    if(file != NULL)
    {
        exit_code = fclose(file);
        if(exit_code < 0)
            return;
    }

    char filename[30];
    sprintf(filename,"his%d",getpid());
    remove(filename);
}

void initiate_client(char* ip_address , unsigned short port)
{
    struct sockaddr_in serverAddr;
    socklen_t addr_size;

    clientSocket = socket(PF_INET, SOCK_STREAM, 0);
    if(clientSocket < 0)
    {
        perror("Socket");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip_address);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    //Connect the socket to the server using the address struct
    addr_size = sizeof serverAddr;
    int status = connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);

    if(status < 0)
    {
        close(clientSocket);
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
}

void authenticate_connection(char* ip_address, char* buffer)
{
    ssize_t received_bytes;

    //Get the client's unique identifier
    int client_id;
    received_bytes = read(clientSocket, &client_id, sizeof(client_id));
    if(received_bytes <= 0)
    {
        printf("Connection terminated\n");
        exit(EXIT_SUCCESS);
    }

    printf("Client ID: #%d\n", client_id);
    printf("Enter PIN code provided by the server\n");
    while(true)
    {
        input_handler(buffer,sizeof(buffer),ip_address);

        unsigned long input_length = strlen(buffer);
        errno = 0;
        char* strtol_check = NULL;

        //Convert string to int
        long int typed_value = strtol(buffer, &strtol_check, 10);

        //Check for validity of input
        if((typed_value == 0 && errno != 0 ) ||
            buffer == strtol_check || //From documentation
            typed_value < 0 || typed_value > 9999 ||
            input_length != 4)
        {
            printf("Invalid password format. Please enter a 4-digit PIN code.\n");
            continue;
        }

        int PIN = (int)typed_value;
        //Send PIN to server
        ssize_t sent_bytes = write(clientSocket, &PIN, sizeof(PIN));

        if(sent_bytes <= 0)
        {
            printf("Connection terminated\n");
            exit(EXIT_SUCCESS);
        }

        //Receive authentication answer from server
        int authentication_byte;
        received_bytes = read(clientSocket, &authentication_byte, sizeof(authentication_byte));
        if(received_bytes <= 0)
        {
            printf("Connection terminated\n");
            exit(EXIT_SUCCESS);
        }

        if(authentication_byte == PIN_SUCCESS)
        {
            printf("Correct PIN code. You can now execute commands.");
            break;
        }
        else if(authentication_byte == PIN_FAILURE)
            printf("Wrong PIN entered, please type again.");
    }
}

int connection_handler(char* ip_address , char *buffer)
{
    char *server_response = NULL;

    while(true)
    {
        ssize_t sent_bytes;

        //Get user's command
        input_handler(buffer, sizeof(buffer),ip_address);

        //If the command is history, print history and proceed to next input
        if(strcmp(buffer,"history") == 0)
        {
            print_history();
            fprintf(file,"%s\n", buffer);
            continue;
        }

        //Send the command to the server
        sent_bytes = send_all(clientSocket, buffer, strlen(buffer) + 1);
        if(sent_bytes < 0)
        {
            puts("Connection has been terminated.");
            return EXIT_FAILURE;
        }

        //If user typed exit then exit the handler
        if(strcmp(buffer,"exit") == 0)
            return EXIT_SUCCESS;

        //Store command to history file
        fprintf(file,"%s\n", buffer);

        //Receive the command result from server
        ssize_t received_bytes =  recv_all(clientSocket,&server_response ,0);
        if(received_bytes <= 0)
        {
            puts("Connection has been terminated.");
            return EXIT_SUCCESS;
        }
        printf("%s",server_response);
        free(server_response);
    }
}

int main(int argc , char* argv[])
{
    if(argc != 3)
    {
        fprintf(stderr,"Usage: ./client [ip address] [port]\n");
        exit(EXIT_FAILURE);
    }

    char* ip_address = argv[1];
    long int port = strtol(argv[2],NULL,10);

    //Quick and dirty check
    if(port <= 0 || port > 65535 )
    {
        fprintf(stderr,"Invalid port argument (1-65535 only)\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE,sigpipe_handler);
    signal(SIGINT,sigint_handler);

    char buffer[BUFFER_SIZE + 1];


    //Initiate connection
    initiate_client(ip_address,(unsigned short)port);
    //Authenticate Connection
    authenticate_connection(ip_address,buffer);

    //Create history file
    create_history();

    //Handle the rest of the connection
    connection_handler(ip_address,buffer);

    //Delete history file
    delete_history();

    return EXIT_SUCCESS;
}
