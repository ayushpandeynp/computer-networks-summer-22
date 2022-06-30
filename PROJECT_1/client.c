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

#define BUFFER_SIZE 1024

#define S_CONTROLPORT 9007 // Control Channel (21 didn't)
#define S_DATAPORT 900     //(NU)// Data Channel

#define C_CONTROLPORT 8081 // NOT USED IN DATA CONNECTION // Client Control Channel PORT - this will be random
#define C_DATAPORT 9090    // USED FOR DATA CHANNEL (CLIENT LISTEN)    // Client Data Channel PORT - this will update based on what we receive from PORT

bool running = true;
int login_state = -1;

void portCommand()
{
    // todo: send port data to server on control channel
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
        if (bind(cSocket, (struct sockaddr *)&sAddr, sizeof(sAddr)) < 0)
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

void performLocalPWD()
{
    char *cwd;
    if ((cwd = getcwd(NULL, 0)))
    {
        char msg[BUFFER_SIZE] = "Current working directory is: ";
        strcat(msg, cwd);
        printf("%s\n", msg);
    }
    else
    {
        printf("An unknown error occurred. Try again!\n");
    }
}

void performLocalCWD(char *buffer)
{
    char *directory = buffer + 5;
    if (chdir(directory) != 0)
    {
        printf("Directory doesn't exist. Try again!\n");
    }
    else
    {
        performLocalPWD();
    }
}

void performLocalLIST()
{
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
                char type = dir->d_type == 4 ? 'D' : 'F';
                printf("%c\t%s\n", type, dir->d_name);
            }
            count++;
        }
        closedir(d);
    }
}

void handleCommands(char buffer[], int cSocket)
{
    char request[BUFFER_SIZE];
    strcpy(request, buffer);

    char command[6];
    strncpy(command, buffer + 0, 5);

    // Client Machine Commands
    if (strstr(command, "QUIT"))
    {
        printf("221 Service closing control connection. \n");
        close(cSocket);
        running = false;
    }
    else if (strstr(command, "!CWD"))
    {
        performLocalCWD(buffer);
    }
    else if (strstr(command, "!PWD"))
    {
        performLocalPWD();
    }
    else if (strstr(command, "!LIST"))
    {
        performLocalLIST();
    }
    // Control Channel Commands
    else
    {
        // send command to server
        char rec_buffer[BUFFER_SIZE];
        bzero(rec_buffer, BUFFER_SIZE);

        send(cSocket, buffer, BUFFER_SIZE, 0);
        bzero(buffer, BUFFER_SIZE);
        recv(cSocket, rec_buffer, BUFFER_SIZE, 0);

        // printing message from server
        printf("%s\n", rec_buffer);

        char statusCode[4];
        bzero(statusCode, 4);
        strncpy(statusCode, rec_buffer, 3);

        if (login_state == -1 && strstr(command, "USER"))
        {
            recv(cSocket, buffer, BUFFER_SIZE, 0);
            printf("%s\n", buffer);

            if (strstr(buffer, "331"))
            {
                login_state++;
            }
            else
            {
                login_state--;
            }
        }
        if (login_state == 0 && strstr(command, "PASS"))
        {
            recv(cSocket, buffer, BUFFER_SIZE, 0);
            printf("%s\n", buffer);

            if (strstr(buffer, "230"))
            {
                login_state++;
            }
            else
            {
                login_state--;
            }
        }
        else if (login_state == 1)
        {
            printf("sts: %s\n", statusCode);
            if (strstr(command, "LIST") && strcmp(statusCode, "150") == 0)
            {
                printf("sds\n");
                int pid = fork();
                printf("sd %d\n", pid);
                if (pid == 0)
                {

                    // data channel is ready
                    int channel = createSocket(true, C_DATAPORT, S_DATAPORT);
                    int client = accept(channel, 0, 0);
                    if (client < 0)
                    {
                        printf("Accept failed.\n");
                        close(channel);
                        exit(EXIT_FAILURE);
                    }
                    close(channel);

                    while (1)
                    {
                        bzero(buffer, BUFFER_SIZE);
                        int bytes = recv(client, buffer, BUFFER_SIZE, 0);
                        if (bytes == 0) // client has closed the connection
                        {
                            close(client);
                            break;
                        }

                        printf("%s \n", buffer);
                    }
                    close(client);
                    exit(EXIT_FAILURE);
                }
            }
            else if (strstr(command, "RETR") && strcmp(statusCode, "150") == 0)
            {
                int pid = fork();
                if (pid == 0)
                {
                    // data channel is ready
                    int channel = createSocket(true, C_DATAPORT, S_DATAPORT);
                    int client = accept(channel, 0, 0);
                    if (client < 0)
                    {
                        printf("Accept failed.\n");
                        close(channel);
                        exit(EXIT_FAILURE);
                    }
                    close(channel);

                    char *filename = request + 5;
                    printf("%s\n", filename);

                    char reader[BUFFER_SIZE];
                    bzero(reader, BUFFER_SIZE);

                    FILE *f;
                    f = fopen(filename, "wb");

                    int total = 0, ln;
                    while ((ln = read(client, reader, BUFFER_SIZE)) > 0)
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

                    close(client);
                    exit(EXIT_FAILURE);
                }
            }
            else if (strstr(command, "STOR") && strcmp(statusCode, "150") == 0)
            {
                int pid = fork();
                if (pid == 0)
                {
                    // data channel is ready
                    int channel = createSocket(true, C_DATAPORT, S_DATAPORT);
                    int client = accept(channel, 0, 0);

                    if (client < 0)
                    {
                        printf("Accept failed.\n");
                        close(channel);
                        exit(EXIT_FAILURE);
                    }

                    char *filename = request + 5;
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

                        printf("%s size is %d bytes.\n", filename, fsize);

                        char databuff[fsize + 1];
                        fread(databuff, 1, sizeof(databuff), f);

                        int total = 0, bytesleft = fsize, ln;
                        while (total < fsize)
                        {
                            ln = write(client, databuff + total, bytesleft);
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
                    close(client);
                    exit(EXIT_FAILURE);
                }
            }
        }
        bzero(buffer, BUFFER_SIZE);
    }
    bzero(buffer, BUFFER_SIZE);
}

int main()
{
    int cSocket = createSocket(false, C_CONTROLPORT, S_CONTROLPORT);

    // accept command
    char buffer[BUFFER_SIZE];
    while (running)
    {
        // take command input
        printf("ftp> ");

        fgets(buffer, BUFFER_SIZE, stdin);

        // remove trailing newline char from buffer, fgets doesn't do it
        buffer[strcspn(buffer, "\n")] = 0; // review 0 or '0'

        handleCommands(buffer, cSocket);
        bzero(buffer, sizeof(buffer));
    }

    return 0;
}