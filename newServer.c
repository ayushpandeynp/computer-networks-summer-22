#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

#define S_CONTROLPORT 21 // Control Channel
#define S_DATAPORT 20    // Data Channel

#define C_PORT 8085 // Client Data Channel PORT - this will update based on what we receive from PORT

int createSocket(bool lstn, int sPort, int cPort);

char *responseMsg(int statusCode)
{
    char *response;
    switch (statusCode)
    {
    case 530:
    {
        char msg[] = "530 Not logged in";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 331:
    {
        char msg[] = "331 Username OK, need password";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 230:
    {
        char msg[] = "230 User logged in, please proceed.";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 503:
    {
        char msg[] = "503 Bad sequence of commands";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 202:
    {
        char msg[] = "202 Command not implemented";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 500:
    {
        char msg[] = "500 Server error, command could not be processed.";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }

    case 150:
    {
        char msg[] = "150 File status okay; about to open data connection.";
        response = (char *)malloc(strlen(msg));
        strcpy(response, msg);
        break;
    }
    }

    return response;
}

void performPWD(int client)
{
    char *cwd;
    if ((cwd = getcwd(NULL, 0)))
    {
        char msg[BUFFER_SIZE] = "Current server working directory is: ";
        strcat(msg, cwd);
        send(client, msg, BUFFER_SIZE, 0);
    }
    else
    {
        send(client, responseMsg(500), BUFFER_SIZE, 0);
    }
}

void performCWD(int client, char *buffer)
{
    char directory[BUFFER_SIZE - 4];
    strncpy(directory, buffer + 4, BUFFER_SIZE - 4);
    if (chdir(directory) != 0)
    {
        send(client, responseMsg(500), BUFFER_SIZE, 0);
    }
    else
    {
        performPWD(client);
    }
}

void performLIST(int client)
{
    send(client, responseMsg(150), BUFFER_SIZE, 0);
    int pid = fork();
    if (pid == 0)
    {
        usleep(1000); // temp
        int channel = createSocket(false, S_DATAPORT, C_PORT);

        char m[256] = "MSG IS HERE";
        send(channel, m, BUFFER_SIZE, 0);

        close(channel);
    }
}

int createSocket(bool lstn, int sPort, int cPort)
{
    // create a socket - Control Channel Socket
    int cSocket;
    cSocket = socket(AF_INET, SOCK_STREAM, 0);

    // check for fail error - for control
    if (cSocket == -1)
    {
        printf("Socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    // setsock
    int value = 1; // scope is closed only until next line
    setsockopt(cSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

    struct sockaddr_in sAddr, cAddr;
    bzero(&sAddr, sizeof(sAddr));
    bzero(&cAddr, sizeof(cAddr));

    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(sPort);
    sAddr.sin_addr.s_addr = INADDR_ANY;

    if (!lstn)
    {
        cAddr.sin_family = AF_INET;
        cAddr.sin_port = htons(cPort);
        cAddr.sin_addr.s_addr = INADDR_ANY;

        // connecting to server port
        int connection_status =
            connect(cSocket,
                    (struct sockaddr *)&cAddr,
                    sizeof(cAddr));

        // check for errors with the connection
        if (connection_status == -1)
        {
            printf("There was an error making a connection to the remote socket. %d %d\n\n", cPort, sPort);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // bind the socket to our specified IP and port
        if (bind(cSocket,
                 (struct sockaddr *)&sAddr,
                 sizeof(sAddr)) < 0)
        {
            printf("Socket bind failed. %d %d\n", cPort, sPort);
            exit(EXIT_FAILURE);
        }
        // after it is bound, we can listen for connections with queue length of 5
        if (listen(cSocket, 5) < 0)
        {
            printf("Listen failed.\n");
            close(cSocket);
            exit(EXIT_FAILURE);
        }

        printf("LISTENING on PORT %d\n", sPort);
    }

    return cSocket;
}

void handleCommands(int client, char *buffer, int *login_state)
{
    char command[6];
    strncpy(command, buffer + 0, 5);

    if (strstr(command, "PWD"))
    {
        performPWD(client);
    }
    else if (strstr(command, "CWD"))
    {
        performCWD(client, buffer);
    }
    else if (strstr(command, "LIST"))
    {
        printf("COMMAND: LIST\n");
        int pid = fork();
        if (pid == 0)
        {
            performLIST(client);
            exit(EXIT_SUCCESS);
        }
    }
    else
    {
        send(client, responseMsg(202), BUFFER_SIZE, 0);
    }
}

int main()
{
    int sSocket = createSocket(true, S_CONTROLPORT, C_PORT);

    // DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
    fd_set all_sockets;
    fd_set ready_sockets;

    // zero out/iniitalize our set of all sockets
    FD_ZERO(&all_sockets);

    // adds one socket (the current socket) to the fd set of all sockets
    FD_SET(sSocket, &all_sockets);

    while (1)
    {
        ready_sockets = all_sockets;
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        for (int fd = 0; fd < FD_SETSIZE; fd++)
        {
            // check to see if that fd is SET
            if (FD_ISSET(fd, &ready_sockets))
            {
                if (fd == sSocket)
                {
                    // accept that new connection
                    int client_sd = accept(sSocket, 0, 0);

                    // add the newly accepted socket to the set of all sockets that we are watching
                    FD_SET(client_sd, &all_sockets);

                    // TODO
                    // add user_data to maintain login_state
                }
                else
                {
                    char buffer[BUFFER_SIZE];
                    bzero(buffer, sizeof(buffer));

                    int bytes = recv(fd, buffer, sizeof(buffer), 0);
                    if (bytes == 0) // client has closed the connection
                    {
                        printf("connection closed from client side \n");
                        // we are done, close fd
                        close(fd);
                        // once we are done handling the connection, remove the socket from the list of file descriptors that we are watching
                        FD_CLR(fd, &all_sockets);
                    }

                    int login_state = -1;

                    printf("RECEIVED CMD: %s\n", buffer);
                    // when data is received
                    handleCommands(fd, buffer, &login_state);
                }
            }
        }
    }

    // close
    close(sSocket);
    return 0;
}