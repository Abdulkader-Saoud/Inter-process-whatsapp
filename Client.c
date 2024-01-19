#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8888

typedef struct {
    int code;
    char message[1024];
} Message;

typedef struct {
    int client_fd;
    int flag;
    Message *message;
} ThreadArgs;

void menu(int client_fd, Message *message, ThreadArgs *args);
int login(int client_fd, char const *id, Message *message);
void receiveMessage(Message *message, int client_fd);
void clearInputBuffer();
void handleSend(int client_fd, Message *message);
void getANewMessage(int client_fd, Message *message);
void getMesaageHistory(int client_fd, Message *message);
void deleteMessageHistory(int client_fd, Message *message);

void* alwaysReceiveMessage(ThreadArgs *args);

int main(int argc, char const *argv[])
{
    int status, valread, client_fd;
    int new = 0;
    struct sockaddr_in serv_addr;
    Message *message = malloc(sizeof(Message));
    pthread_t thread;
    ThreadArgs *args = malloc(sizeof(ThreadArgs));

    if (argv[1] == NULL || strlen(argv[1]) < 2)
    {
        printf("Please enter your ID\nMust be at least 2 characters long\n");
        return -1;
    }

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    

    new = login(client_fd, argv[1], message);

    
    args->client_fd = client_fd;
    args->message = malloc(sizeof(Message));
    args->message->code = 1;

    if (pthread_create(&thread, NULL, (void *)alwaysReceiveMessage, (void *)args) < 0)
    {
        perror("could not create thread");
        return 1;
    }
    
    menu(client_fd, message, args);
    printf("Disconnected from server\n");
    close(client_fd);
    
    printf("Client closed\n");
    pthread_join(thread, NULL);

    free(message);
    free(args->message);
    free(args);
    return 0;
}
void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
void menu(int client_fd, Message *message, ThreadArgs *args)
{
    int choice;
    char tmp[1024];
    while (1)
    {
        printf("-------- MENU --------\n");
        printf("0. Exit\n");
        printf("1. Add Contact\n");
        printf("2. Delete Contact\n");
        printf("3. List Contacts\n");
        printf("4. Send Message\n");
        printf("5. Check Messages\n");
        printf("6. Read a New Message\n");
        printf("7. Message History\n");
        printf("8. Delete message history\n");
        printf("*|  Enter your choice: \n");
        fflush(stdin);
        scanf("%d", &choice);
        clearInputBuffer();
        
        switch (choice)
        {
        case 0:
            message->code = 0;
            send(client_fd, message, sizeof(Message), 0);
            return;
            break;
        case 1:
            printf("*|  Enter contact id: ");
            fgets(tmp, sizeof(tmp), stdin);
            tmp[strlen(tmp) - 1] = '\0';
            message->code = 1;
            break;
        case 2:
            printf("*|  Enter contact id: ");
            fgets(tmp, sizeof(tmp), stdin);
            tmp[strlen(tmp) - 1] = '\0';
            message->code = 2;
            break;
        case 3:
            message->code = 3;
            message->message[0] = '\0';
            break;
        case 4:
            handleSend(client_fd, message);
            break;
        case 5:
            message->code = 5;
            message->message[0] = '\0';
            break;
        case 6:
            getANewMessage(client_fd, message);
            break;
        case 7:
            getMesaageHistory(client_fd, message);
            break;
        case 8:
            deleteMessageHistory(client_fd, message);
            break;
        
        default:
            printf("Invalid choice\n");
            break;
        }
        if (choice != 4 && choice != 6 && choice != 7 && choice != 8)
            strcpy(message->message, tmp);
        
        send(client_fd, message, sizeof(Message), 0);
        args->flag = 0;
        while (!args->flag);
    }
}

void* alwaysReceiveMessage(ThreadArgs *args)
{
    int client_fd = args->client_fd;
    Message *message = args->message;

    while (message->code != 0)
    {
        receiveMessage(message, client_fd);
        printf("S| %s\n", message->message);
        if (message->code != 444 && message->code != 896){
            args->flag = 1;
        }
    }
    return NULL;
}

void deleteMessageHistory(int client_fd, Message *message)
{
    char tmp[1024];
    printf("Enter Contact id: ");
    fgets(tmp, sizeof(tmp), stdin);
    tmp[strlen(tmp) - 1] = '\0';
    strcpy(message->message, tmp);
    message->code = 70;
}
void getMesaageHistory(int client_fd, Message *message)
{
    char tmp[1024];
    printf("Enter Contact id: ");
    fgets(tmp, sizeof(tmp), stdin);
    tmp[strlen(tmp) - 1] = '\0';
    strcpy(message->message, tmp);
    message->code = 60;
}
void getANewMessage(int client_fd, Message *message)
{
    char tmp[1024];
    printf("Which message do you want to get? ");
    fgets(tmp, sizeof(tmp), stdin);
    tmp[strlen(tmp) - 1] = '\0';
    strcpy(message->message, tmp);
    message->code = 6;
}

void handleSend(int client_fd, Message *message)
{
    char id[25];
    char tmp[999];
    printf("Enter Reciever id: ");
    fgets(id, sizeof(id), stdin);
    id[strlen(id) - 1] = '\0';
    printf("Enter Message: ");
    fgets(tmp, sizeof(tmp), stdin);
    tmp[strlen(tmp) - 1] = '\0';
    sprintf(message->message, "%s|%s", id, tmp);
    message->code = 4;
}

void receiveMessage(Message *message, int client_fd){
    recv(client_fd, message, sizeof(Message), 0);
    message->message[1023] = '\0';
}

int login(int client_fd, char const *id, Message *message)
{
    char tmp[1024];
    int new = 0;
    strcpy(message->message, id);
    send(client_fd, message, sizeof(Message), 0);
    receiveMessage(message, client_fd);
    if (message->code == 201)
    {
        new = 1;
        do
        {
            printf("S| %s\n", message->message);
            printf("Enter your Name Surname Number: ");
            fgets(tmp, sizeof(tmp), stdin);
            tmp[strlen(tmp) - 1] = '\0';
            strcpy(message->message, tmp);
            send(client_fd, message, sizeof(Message), 0);
            receiveMessage(message, client_fd);
        } while (message->code != 200);
    }
    printf("S| %s\n", message->message);
    printf("\n");
    return new;
}


