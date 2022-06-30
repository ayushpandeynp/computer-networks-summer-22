#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

#define S_CONTROLPORT 31 // Control Channel
#define S_DATAPORT 30    // Data Channel

#define C_PORT 8086 // Client Data Channel PORT - this will update based on what we receive from PORT

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
    char *directory = buffer + 4;
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
    usleep(1000);
    int channel = createSocket(false, S_DATAPORT, C_PORT);

    int count = 0;
    struct dirent *dir;

    DIR *d;
    d = opendir(".");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (count > 1)
            {
                char line[BUFFER_SIZE];
                char type[] = {dir->d_type == 4 ? 'D' : 'F', '\0'};
                strcpy(line, type);
                strcat(line, "\t");
                strcat(line, dir->d_name);
                send(channel, line, BUFFER_SIZE, 0);
            }
            count++;
        }
        closedir(d);
    }

    close(channel);
}

void performRETR(int client, char *filename)
{
    send(client, responseMsg(150), BUFFER_SIZE, 0);
    usleep(1000);
    int channel = createSocket(false, S_DATAPORT, C_PORT);

    FILE *f;
    f = fopen(filename, "rb");
    if (f == NULL)
    {
        printf("Can't open %s\n", filename);
    }
    else
    {
        struct stat stat_buf;
        int rc = stat(filename, &stat_buf);
        int fsize = stat_buf.st_size;

        char *databuff[fsize + 1];
        fread(databuff, 1, sizeof(databuff), f);

        int total = 0, bytesleft = fsize, ln;

        while (total < fsize)
        {
            ln = send(channel, databuff + total, bytesleft, 0);
            if (ln == -1)
            {
                break;
            }
            total += ln;
            bytesleft -= ln;
        }
        bzero(databuff, sizeof(databuff));
        fclose(f);
    }
    close(channel);
}

void performSTOR(int client, char *filename)
{
    send(client, responseMsg(150), BUFFER_SIZE, 0);
    usleep(1000);
    int channel = createSocket(false, S_DATAPORT, C_PORT);

    printf("%s\n", filename);

    char reader[BUFFER_SIZE];
    bzero(reader, BUFFER_SIZE);

    FILE *f;
    f = fopen(filename, "wb");

    int total = 0, ln;
    while ((ln = read(channel, reader, BUFFER_SIZE)) > 0)
    {
        fwrite(reader, 1, BUFFER_SIZE, f);
        total += ln;
        if (ln < BUFFER_SIZE)
        {
            break;
        }
    }
    printf("Total data received for %s = %d bytes.\n", filename, total);
    fclose(f);
    close(channel);
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

        printf("LISTENING on PORT %d...\n", sPort);
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
        int pid = fork();
        if (pid == 0)
        {
            performLIST(client);
            exit(EXIT_SUCCESS);
        }
    }
    else if (strstr(command, "RETR"))
    {
        int pid = fork();
        if (pid == 0)
        {
            performRETR(client, buffer + 5);
            exit(EXIT_SUCCESS);
        }
    }
    else if (strstr(command, "STOR"))
    {
        int pid = fork();
        if (pid == 0)
        {
            performSTOR(client, buffer + 5);
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

                    int login_state = -1; // todo

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