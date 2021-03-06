#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include "common.h"

#define MAX_BUFFER_SIZE 500
#define NUMBER_OF_USERS 1000
int current_number_of_users = 0;

int *connections;
struct sockaddr_in server_addr, client_addr;

int socket_desc, client_sock, socket_length, *new_sock;

typedef struct userList {
    char username[5];
    char password[100];
} userList;


userList existing_users[] = {
        {"user\0", "123"},
        {"admi\0", "12"},
        {"gigi\0", "asds"}
};

userList connected_users[NUMBER_OF_USERS];

void broadcast_message(message message_received);

void initialize_server() {
    connections = calloc(NUMBER_OF_USERS, sizeof(int));
    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
        exit(1);
    }
    puts("Socket created");

    //Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //Bind
    if (bind(socket_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        //print the error message
        perror("bind failed. Error");
        exit(2);
    }
    puts("bind done");
    //Listen
    listen(socket_desc, 10);// NUMBER_OF_USERS);
}


void authentication(int sock, message *message_received) {
    int flag = 0; //error
    char *message_to_send;
    //checks if user is in the existing list of users
    for (int i = 0; i < 3; i++) {
        if ((strncmp(existing_users[i].username, (*message_received).username, 4) == 0)
            && (strcmp(existing_users[i].password, (*message_received).body) == 0)) {
            flag = 1;
            break;

        }
    }
    if (flag == 1) {
        //checks if there is any other user connected with the same username
        for (int i = 0; i < current_number_of_users; i++) {
            if (strncmp(connected_users[i].username, (*message_received).username, 4) == 0) {
                flag = 0;
            }
        }
    }
    if (flag == 1) {
        strcpy(connected_users[current_number_of_users].username, (*message_received).username);
        strcpy(connected_users[current_number_of_users].password, (*message_received).body);
        current_number_of_users++;

        message ack_success_msg = create_message("ACK", (*message_received).username, "");
        message_to_send = serialize(ack_success_msg);
    } else {
        message ack_error_msg = create_message("ERR", (*message_received).username, "");
        message_to_send = serialize(ack_error_msg);
    }
    write(sock, message_to_send, strlen(message_to_send));
}

void connection_handler(void *socket_desc) {

    //Get the socket descriptor
    int sock = *(int *) socket_desc;
    int read_size;
    char buffer_from_clients[MAX_BUFFER_SIZE];

    //Receive a message from client
    while (1) {
        memset(buffer_from_clients, 0, MAX_BUFFER_SIZE);
        message message_received;
        if ((read_size = (int) recv(sock, buffer_from_clients, TOTAL_HEADER_LEN, 0)) > 0) {
            printf("Message received from cliend id: %d, %s\n", sock, buffer_from_clients);
            message_received = deserialize(buffer_from_clients);
        }
        memset(buffer_from_clients, 0, MAX_BUFFER_SIZE);
        if (read_size > 0 &&
            message_received.length > 0 &&
            (read_size = (int) recv(sock, buffer_from_clients, message_received.length, 0)) > 0) {

            strcpy(message_received.body, buffer_from_clients);
        }

        if (read_size > 0) {

            if (strcmp(message_received.type, "AUT") == 0) {
                authentication(sock, &message_received);

            } else if (strcmp(message_received.type, "MSG") == 0) {
                broadcast_message(message_received);

            } else if (strcmp(message_received.type, "DIS") == 0) {
                message ack_disconnect = create_message("DIS", message_received.username, "");
                char *message_to_send = serialize(ack_disconnect);
                write(sock, message_to_send, strlen(message_to_send));

                printf("Log out\n");

                //remove user from connected users
                for(int i=0;i<current_number_of_users;i++){
                    if(strcmp(connected_users[i].username,message_received.username) == 0){
                        for(int j=i+1; j<current_number_of_users;j++){
                            connected_users[j-1] = connected_users[j];
                        }
                        current_number_of_users--;
                        break;
                    }
                }

            }
        }
    }
}

void broadcast_message(message message_received) {
    for (int index = 0; index < current_number_of_users; index++) {
        if (connections[index] > 0) {
            printf("Client: %d\n", connections[index]);

            char *message_to_send = serialize(message_received);
            write(connections[index], message_to_send, strlen(message_to_send));
        }
    }
}


int main(int argc, char *argv[]) {

    initialize_server();
    userList connected_users[NUMBER_OF_USERS];
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    socket_length = sizeof(struct sockaddr_in);

    while ((connections[current_number_of_users] = accept(socket_desc, (struct sockaddr *) &client_addr,
                                                          (socklen_t *) &socket_length))) {
        puts("Connection accepted");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = connections[current_number_of_users];

        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void *) new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }

        puts("Handler assigned");
        //current_number_of_users++;
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;

}
