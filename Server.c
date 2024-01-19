#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>

/*
    to fix
    
    SERVER
    403 - Error
    200 - OK
    201 - New Account
    100 - New Message
    444 - to be continued
    777 - get new messages operation done

    Client
    0 - Exit
    1: Add Contact
    2: Delete Contact
    3: Get Contacts
    4: Send Message
    5: Get New Messages
    6: Get A New Message
    60: Get Message History
    70: Delete Message History
*/

typedef struct User User;
struct User{
    char name[25];
    char surname[25];
    char id[25];
    char number[25];
    int contacts_count;
    int socket;
    pthread_mutex_t fileMutex;
    User **contacts;
};

typedef struct {
    User **array;
    int size;
    int count;
    pthread_mutex_t mutex;
} Users;

typedef struct {
    int code;
    char message[1024];
} Message;

struct ThreadArgs {
    int socket;
    Users *users;
};

void *connection_handler(void *);

User* newUser(Message *message, char id[25]);
void createUserDirectory(User *user);
void readUserFile(User *user, char dirName[25]);
void readAllUsers(Users **users);
void addUserToArray(Users *users, User *user);
void freeUsers(Users *users);
int checkIfUserExists(Users *users, char id[25]);
void receiveMessage(Message *message, int sock);
User* handleLogin(int sock, Users *users, Message *message);

void addContact(Users *users,User *user, Message *message);
void deleteContact(Users *users,User *user, Message *message);
void readContacts(User *user, Users *users);
void writeContacts(User *user);
void getContacts(User* user, Message *message);
int checkIfExistsInContacts(User *user, char id[25]);

void handleSend(int sock, Message *message, Users *users, User *user);
void writeNewMessage(char idSender[25], char idReciever[25], char msg[1024], int flag);
void getNewMessages(User *user, Message *message);
void writeToNew(char idSender[25], char idReciever[25], char msg[1024]);
void getANewMessage(User *user, Message *message);
void getMessageHistory(User *user, Message *message);
void deleteMessageHistory(User *user, Message *message);



int main(int argc, char *argv[])
{
    Users *users;
    int socket_desc, new_socket, c;
    struct sockaddr_in server, client;
    int i;
    readAllUsers(&users);
    if (pthread_mutex_init(&(users->mutex), NULL) != 0)
    {
        printf("Mutex initialization failed\n");
        return 1;
    }

    
    printf("Current Users Count: %d\n", users->count);

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        puts("bind failed");
        return 1;
    }
    puts("bind done");

    listen(socket_desc, 50);

    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while (1)
    {
        new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
        if (new_socket < 0)
        {
            perror("accept failed");
            continue;
        }
        puts("Connection accepted");

        pthread_t sniffer_thread;
        struct ThreadArgs *threadArgs = malloc(sizeof(struct ThreadArgs));
        threadArgs->socket = new_socket;
        threadArgs->users = users;

        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void *)threadArgs) < 0)
        {
            perror("could not create thread");
        }
       
        puts("Handler assigned");
    }

    printf("Server disconnected\n");

    pthread_mutex_destroy(&(users->mutex));
    for (i = 0; i < users->count; i++)
    {
        pthread_mutex_destroy(&(users->array[i]->fileMutex));
        free(users->array[i]);
    }
    freeUsers(users);
    return 0;
}


void *connection_handler(void *args)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int sock = threadArgs->socket;
    Users *users = threadArgs->users;
    int read_size;
    Message *message = malloc(sizeof(Message));
    User *user;
    user = handleLogin(sock, users, message);
    if (user == NULL) {
        printf("Client disconnected\n");
        close(sock);
        free(message);
        free(args);
        return NULL;
    }
    user->socket = sock;
    readContacts(user, users);
    receiveMessage(message, sock);

    while(message->code != -1){
        switch (message->code) {
            case 0:
                message->code = 0;
                strcpy(message->message, "Goodbye");
                break;
            case 1:
                addContact(users, user, message);
                break;
            case 2:
                deleteContact(users, user, message);
                break;
            case 3:
                getContacts(user, message);
                break;
            case 4:
                handleSend(sock, message, users, user);
                break;
            case 5:
                getNewMessages(user, message);
                break;
            case 6:
                getANewMessage(user, message);
                break;
            case 60:
                getMessageHistory(user, message);
                break;
            case 70:
                deleteMessageHistory(user, message);
                break;
            default:
                break;
        }
        send(sock, message, sizeof(Message), 0);
        message->message[1023] = '\0';
        if (message->code != 0)
            receiveMessage(message, sock);
        else
            message->code = -1;
    }
    user->socket = -1;
    writeContacts(user);
    free(message);
    close(sock);
    free(args);
    printf("Client disconnected %s\n", user->name);
    return NULL;
}
int checkIfExistsInContacts(User *user, char id[25]) {
    int i;
    for (i = 0; i < user->contacts_count; i++) {
        if (strcmp(user->contacts[i]->id, id) == 0) {
            return 1;
        }
    }
    return 0;
}
void deleteMessageHistory(User *user,Message *message) {
    FILE *file;
    char filePath[60];
    char id[25], msg[1024];

    strncpy(id, message->message, sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';

    sprintf(filePath, "%s/%s.txt", user->id, id);
    file = fopen(filePath, "r");
    if (file == NULL) {
        message->code = 403;
        strcpy(message->message, "There in no History...");
        return;
    }
    fclose(file);
    pthread_mutex_lock(&(user->fileMutex));
    remove(filePath);
    pthread_mutex_unlock(&(user->fileMutex));
    message->code = 200;
    strcpy(message->message, "History Deleted");
}

void getMessageHistory(User *user, Message *message) {
    FILE *file;
    char filePath[60];
    char id[25],msg[1024];

    strncpy(id, message->message, sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    if (strcmp(id, user->id) == 0) {
        strcpy(message->message, "You Cant Contact Yourself");
        message->code = 403;
        return;
    }
    sprintf(filePath, "%s/%s.txt", user->id, id);

    
    file = fopen(filePath, "r");
    if (file == NULL) {
        message->code = 403;
        strcpy(message->message, "There in no History...");
        return;
    }
    pthread_mutex_lock(&(user->fileMutex));
    while (fscanf(file, "%[^\n]\n", msg) > 0) {
        sprintf(message->message, "%s", msg);
        message->code = 444;
        send(user->socket, message, sizeof(Message), 0);
    }
    fclose(file);
    pthread_mutex_unlock(&(user->fileMutex));

    sprintf(message->message, "... ... ...");
    message->code = 200;
}
void handleSend(int sock, Message *message, Users *users, User *user) {
    char id[25];
    char msg[1024];
    int tmp;
    User* reciever;
    sscanf(message->message, "%[^|]|%[^\n]", id, msg);
    pthread_mutex_lock(&(users->mutex));
    printf("Sending message to %s\n", id);
    printf("Message: %s\n", msg);
    if (strcmp(id, user->id) == 0) {
        strcpy(message->message, "You can't send message to yourself");
        message->code = 403;
        return;
    }
    tmp = checkIfExistsInContacts(user, id);
    pthread_mutex_unlock(&(users->mutex));
    if (tmp == 0) {
        strcpy(message->message, "User does not exist in Contacts");
        message->code = 403;
    } else {
        tmp = checkIfUserExists(users, id);
        reciever = users->array[tmp];
        pthread_mutex_lock(&(reciever->fileMutex));
        
        writeToNew(user->id, reciever->id, msg);
        writeNewMessage(user->id, reciever->id, msg, 0);
        writeNewMessage(reciever->id,user->id, msg, 1);
        if (reciever->socket != -1) {
            sprintf(message->message, "You have new Message from %s", user->id);
            message->code = 896;
            send(reciever->socket, message, sizeof(Message), 0);
        }
        pthread_mutex_unlock(&(reciever->fileMutex));

        sprintf(message->message, "Message sent to %s", id);
        message->code = 200;
    }
}
void writeToNew(char idSender[25], char idReciever[25], char msg[1024]) {
    FILE *file;
    char filePath[50];
    sprintf(filePath, "%s/new.txt", idReciever);
    file = fopen(filePath, "a");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s| %s\n", idSender, msg);
    fclose(file);
}
void writeNewMessage(char idSender[25], char idReciever[25], char msg[1024], int flag) {
    FILE *file;
    char filePath[50];
    sprintf(filePath, "%s/%s.txt", idSender, idReciever);
    file = fopen(filePath, "a");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    if (!flag)
        fprintf(file, "You| %s\n", msg);
    else
        fprintf(file, "%s| %s\n", idReciever, msg);
    
    fclose(file);
}
void getNewMessages(User *user, Message *message) {
    FILE *file;
    char filePath[50];
    char id[25], msg[1024];
    sprintf(filePath, "%s/new.txt", user->id);

    pthread_mutex_lock(&(user->fileMutex));
    file = fopen(filePath, "a+");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    while (fscanf(file, "%[^|]| %[^\n]\n", id, msg) > 0) {
        sprintf(message->message, "You have new Message from %s", id);
        message->code = 444;
        send(user->socket, message, sizeof(Message), 0);
    }
    fclose(file);
    pthread_mutex_unlock(&(user->fileMutex));

    sprintf(message->message, "... ... ...");
    message->code = 777;
    
}

void getANewMessage(User *user, Message *message) {
    FILE *file;
    FILE *tempFile;
    char filePath[50];
    char id[25], msg[1024],idSender[25];

    pthread_mutex_lock(&(user->fileMutex));
    sprintf(filePath, "%s/new.txt", user->id);
    file = fopen(filePath, "a+");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    strncpy(id, message->message, sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';

    tempFile = fopen("temp.txt", "w");
    if (tempFile == NULL) {
        perror("Error opening temporary file");
        exit(EXIT_FAILURE);
    }

    while (fscanf(file, "%[^|]| %[^\n]\n", idSender, msg) > 0) {
        if (strcmp(idSender, id) == 0) {
            message->code = 444;
            strcpy(message->message, msg);
            send(user->socket, message, sizeof(Message), 0);
        }
        else {
            fprintf(tempFile, "%s| %s\n", idSender, msg);
        }
    } 
    
    fclose(file);
    fclose(tempFile);
    remove(filePath);
    rename("temp.txt", filePath);
    pthread_mutex_unlock(&(user->fileMutex));
    sprintf(message->message, "... ... ...");
    message->code = 200;
    
}

void getContacts(User* user, Message *message) {
    char contacts[1024];
    strcpy(contacts, "");
    if (user->contacts_count == 0) {
        strcpy(message->message, "You have no contacts");
        message->code = 403;
        return;
    }
    for (int i = 0; i < user->contacts_count; i++) {
        strcat(contacts, user->contacts[i]->id);
        strcat(contacts, " ");
        strcat(contacts, user->contacts[i]->name);
        strcat(contacts, " \n");
    }
    strcpy(message->message, contacts);
    message->message[strlen(message->message)] = '\0';
    message->code = 200;
}

void addContact(Users *users, User *user, Message *message) {
    char id[25];
    int tmp,i;
    User *contact;
    sscanf(message->message, "%s", id);

    if (strcmp(id, user->id) == 0) {
        strcpy(message->message, "You can't add yourself");
        message->code = 403;
        return;
    }
    i = 0;
    while (i < user->contacts_count) {
        if (strcmp(user->contacts[i]->id, id) == 0) {
            strcpy(message->message, "User already in Contacts");
            message->code = 403;
            return;
        }
        i++;
    }
    pthread_mutex_lock(&(users->mutex));
    tmp = checkIfUserExists(users, id);

    if (tmp == -1) {
        strcpy(message->message, "User does not exist");
        message->code = 403;
    } else {
        contact = malloc(sizeof(User));
        if (contact == NULL) {
            perror("Memory allocation error");
            exit(EXIT_FAILURE);
        }

        strcpy(contact->name, users->array[tmp]->name);
        strcpy(contact->surname, users->array[tmp]->surname);
        strcpy(contact->id, users->array[tmp]->id);
        strcpy(contact->number, users->array[tmp]->number);
        
        user->contacts_count++;
        user->contacts = realloc(user->contacts, user->contacts_count * sizeof(User *));
        user->contacts[user->contacts_count - 1] = contact;

        sprintf(message->message, "%s added to Contacts", contact->name);
        message->code = 200;
        
    }

    pthread_mutex_unlock(&(users->mutex));
}


void deleteContact(Users *users, User *user, Message *message) {
    char id[25];
    int i = 0;

    sscanf(message->message, "%s", id);
    while (i < user->contacts_count) {
        if (strcmp(user->contacts[i]->id, id) == 0) {
            free(user->contacts[i]);
            user->contacts_count--;
            user->contacts[i] = user->contacts[user->contacts_count];
            user->contacts = realloc(user->contacts, user->contacts_count * sizeof(User *));
            i = user->contacts_count + 1;
        }
        i++;
    }
    if (i == user->contacts_count) {
        strcpy(message->message, "User does not exist in Contacts");
        message->code = 403;
        return;
    }
    sprintf(message->message, "%s deleted from Contacts", id);
    message->code = 200;
}

void readContacts(User *user, Users *users) {
    FILE *file;
    Message *message = malloc(sizeof(Message));
    
    char filePath[50];
    char contactId[25];
    int tmp;
    sprintf(filePath, "%s/contacts.txt", user->id);
    file = fopen(filePath, "r");
    if (file == NULL) {
        perror("Error opening contacts file");
        exit(EXIT_FAILURE);
    }
    user->contacts_count = 0;
    user->contacts = malloc(sizeof(User *));
    while (fscanf(file, "%s", contactId) == 1) {
        strcpy(message->message, contactId);
        addContact(users, user, message);
    }
    fclose(file);
}

void writeContacts(User *user) {
    FILE *file;
    char filePath[50];

    sprintf(filePath, "%s/contacts.txt", user->id);
    file = fopen(filePath, "w");
    if (file == NULL) {
        perror("Error opening contacts file");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < user->contacts_count; i++) {
        fprintf(file, "%s\n", user->contacts[i]->id);
        free(user->contacts[i]);
    }
    fclose(file);
}

void receiveMessage(Message *message, int sock){
    recv(sock, message, sizeof(Message), 0);
    message->message[1023] = '\0';
}
User* handleLogin(int sock, Users *users, Message *message) {
    char id[25];
    User *user = NULL;
    int tmp;

    receiveMessage(message, sock);
    sscanf(message->message, "%s", id);
    printf("Client with ID %s connected\n", id);

    pthread_mutex_lock(&(users->mutex));

    tmp = checkIfUserExists(users, id);

    if (tmp == -1) {
        strcpy(message->message, "Making new Account");
        message->code = 201;
    }

    while (tmp == -1) {
        send(sock, message, sizeof(Message), 0);
        receiveMessage(message, sock);
        user = newUser(message, id);
        if (user != NULL) {
            addUserToArray(users, user);
            tmp = users->count - 1;
        }
    }

    user = users->array[tmp];
    sprintf(message->message, "Welcome %s", user->name);
    message->code = 200;
    send(sock, message, sizeof(Message), 0);

    pthread_mutex_unlock(&(users->mutex));
    return user;
}

int checkIfUserExists(Users *users, char id[25])
{
    int i;
    for (i = 0; i < users->count; i++)
    {
        if (strcmp(users->array[i]->id, id) == 0)
        {
            return i;
        }
    }
    return -1;
}

User* newUser(Message *message, char id[25])
{
    User *user;
    
    char name[25], surname[25], number[25];
    sscanf(message->message, "%s %s %s",name, surname, number);
    if (strcmp(name, "NULL") == 0 || strlen(name) < 2)
    {
        strcpy(message->message, "Error in Your name");
        message->code = 403;
        return NULL;
    }
    else if (strcmp(surname, "NULL") == 0 || strlen(surname) < 2)
    {
        strcpy(message->message, "Error in Your surname");
        message->code = 403;
        return NULL;
    }
    else if (strcmp(number, "NULL") == 0 || strlen(number) < 2)
    {
        strcpy(message->message, "Error in Your number");
        message->code = 403;
        return NULL;
    }
    else
    {
        user = malloc(sizeof(User));
        user->contacts_count = 0;
        strcpy(user->name, name);
        strcpy(user->surname, surname);
        strcpy(user->number, number);
        strcpy(user->id, id);
        createUserDirectory(user);
        strcpy(message->message, "User created");
        message->code = 200;

        if (pthread_mutex_init(&(user->fileMutex), NULL) != 0) {
            perror("Mutex initialization failed");
            exit(EXIT_FAILURE);
        }
        return user;
    }
}

void readAllUsers(Users **users)
{
    DIR *dir;
    struct dirent *entry;
    char dirName[25];
    User *user;
    (*users) = malloc(sizeof(Users));
    (*users)->size = 10;
    (*users)->array = malloc((*users)->size * sizeof(User *));
    (*users)->count = 0;

    dir = opendir(".");
    if (dir == NULL)
    {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            strncpy(dirName, entry->d_name, sizeof(dirName) - 1);
            dirName[sizeof(dirName) - 1] = '\0';

            user = malloc(sizeof(User));
            readUserFile(user, dirName);
            user->socket = -1;
            addUserToArray(*users, user);

            //printf("User ID: %s\nName: %s\nSurname: %s\nNumber: %s\n\n", user->id, user->name, user->surname, user->number);
        }
    }

    closedir(dir);
}

void addUserToArray(Users *users, User *user)
{
    if (users->count == users->size)
    {
        users->size *= 2;
        users->array = realloc(users->array, users->size * sizeof(User *));
    }
    users->array[users->count] = user;
    users->count++;
}

void readUserFile(User *user, char dirName[25])
{
    FILE *file;
    char filePath[50];
    sprintf(filePath, "%s/user.txt", dirName);

    file = fopen(filePath, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "Name: %s\n", user->name);
    fscanf(file, "Surname: %s\n", user->surname);
    fscanf(file, "ID: %s\n", user->id);
    fscanf(file, "Number: %s\n", user->number);

    fclose(file);
}

void createUserDirectory(User *user)
{
    FILE *file;
    char filePath[50];
    char dirName[25];
    struct stat st = {0};

    sprintf(dirName, "%s", user->id);

    if (stat(dirName, &st) == -1)
    {
        mkdir(dirName, 0700);
    }

    sprintf(filePath, "%s/user.txt", dirName);

    file = fopen(filePath, "w");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "Name: %s\n", user->name);
    fprintf(file, "Surname: %s\n", user->surname);
    fprintf(file, "ID: %s\n", user->id);
    fprintf(file, "Number: %s\n", user->number);
    fclose(file);
    sprintf(filePath, "%s/contacts.txt", dirName);
    file = fopen(filePath, "w");
    if (file == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void freeUsers(Users *users)
{
    int i;
    for (i = 0; i < users->count; ++i)
    {
        free(users->array[i]);
    }
    free(users->array);
}
