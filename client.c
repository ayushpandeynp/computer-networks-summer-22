#include <stdio.h>
#include <strings.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>

#define BUFFER_SIZE 1024

#define S_CONTROLPORT 21 // Control Channel
#define S_DATAPORT 20    // Data Channel

#define C_CONTROLPORT 8081 // Client Control Channel PORT - this will be random
#define C_DATAPORT 8083    // Client Data Channel PORT - this will update based on what we receive from PORT

bool running = true;
int login_state = 1;

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
        printf("socket creation failed..\n");
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

        // binding client port
        if (bind(cSocket, (struct sockaddr *)&cAddr, sizeof(struct sockaddr_in)) != 0)
        {
            printf("Could not bind.\n");
        }

        // connecting to server port
        int connection_status =
            connect(cSocket,
                    (struct sockaddr *)&sAddr,
                    sizeof(sAddr));

        // check for errors with the connection
        if (connection_status == -1)
        {
            printf("There was an error making a connection to the remote socket. \n\n");
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
            printf("socket bind failed..\n");
            exit(EXIT_FAILURE);
        }

        // after it is bound, we can listen for connections with queue length of 5
        if (listen(cSocket, 5) < 0)
        {
            printf("Listen failed..\n");
            close(cSocket);
            exit(EXIT_FAILURE);
        }

        printf("Server is listening on PORT [%d]\n", sPort);
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
    char directory[BUFFER_SIZE - 5];
    strncpy(directory, buffer + 5, BUFFER_SIZE - 5);
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
        send(cSocket, buffer, BUFFER_SIZE, 0);
        bzero(buffer, BUFFER_SIZE);
        recv(cSocket, buffer, BUFFER_SIZE, 0);

        // printing message from server
        printf("%s\n", buffer);

        char statusCode[4];
        strncpy(statusCode, buffer + 0, 3);

        if (login_state == -1 && strstr(command, "USER") && strcmp(statusCode, "331"))
        {
            // if (strstr(buffer, "STOR") != NULL)
            // {
            // 	send(cSocket, buffer, strlen(buffer), 0);
            // 	int datafd = dataconn();

            // 	char *filename = buffer + 5;
            // 	// printf("FN: %s\n",filename);

            // 	FILE *fp;
            // 	fp = fopen(filename, "rb");
            // 	// char *temp, *filename = buffer+5;

            // 	if (fp == NULL)
            // 	{
            // 		printf("can't open %s\n", filename);
            // 	}
            // 	printf("Sending STOR Request for %s.\n", filename);
            // 	write(cSocket, buf, sizeof(buf));
            // 	struct stat stat_buf;
            // 	int rc = stat(filename, &stat_buf);
            // 	int fsize = stat_buf.st_size;
            // 	printf("%s size is %d bytes.\n", filename, fsize);
            // 	char *databuff[fsize + 1];
            // 	fread(databuff, 1, sizeof(databuff), fp);
            // 	int total = 0, bytesleft = fsize, ln;
            // 	printf("Sending %s......\n", filename);

            // 	while (total < fsize)
            // 	{
            // 		printf("%ld \n", sizeof(databuff));
            // 		ln = send(datafd, databuff + total, bytesleft, 0);
            // 		printf("%ld \n", sizeof(ln));
            // 		if (ln == -1)
            // 			break;
            // 		total += ln;
            // 		bytesleft -= ln;
            // 	}
            // 	printf("Total data sent for %s = %d bytes.\n", filename, total);
            // 	bzero(databuff, sizeof(databuff));
            // 	fclose(fp);

            // 	bzero(buffer, sizeof(buffer));
            // }
        }
        else if (login_state == 0 && strstr(command, "PASS") && strcmp(statusCode, "230"))
        {
            login_state++;
        }
        else if (login_state == 1)
        {
            if (strstr(command, "LIST") && strcmp(statusCode, "150") == 0)
            {

                int pid = fork();
                if (pid == 0)
                {
                    // data channel is ready
                    int channel = createSocket(true, C_DATAPORT, S_DATAPORT);
                    int client = accept(channel, 0, 0);
                    if (client < 0)
                    {
                        printf("Accept failed..\n");
                        close(channel);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        printf("ACCEPTED CONNECTION.\n");
                        send(cSocket, "YES", 4, 0);
                    }

                    bzero(buffer, BUFFER_SIZE);
                    int bytes = recv(client, buffer, BUFFER_SIZE, 0);
                    printf("RECEIVED: %s\n", buffer);
                    if (bytes == 0) // server has closed the connection
                    {
                        printf("connection closed from server side \n");
                        close(channel);
                    }

                    printf("%s\n", buffer);
                }
            }
            else if (strstr(command, "RETR") && strcmp(statusCode, "150") == 0)
            {
                printf("DATA CHANNEL: RETR");
                int channel = createSocket(true, S_DATAPORT, C_DATAPORT);
            }
            else if (strstr(command, "STOR") && strcmp(statusCode, "150") == 0)
            {
                // int channel = createSocket(false, C_PORT);
            }
        }
    }
    bzero(buffer, BUFFER_SIZE);
}

int main()
{
    int cSocket = createSocket(false, S_CONTROLPORT, 0);

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
    }

    return 0;
}
