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

#define S_PORT 21   // Control Channel PORT
#define C_PORT 8080 // Data Channel PORT - should be random later

bool running = true;
int login_state = -1;

void portCommand()
{
    // todo: send port data to server on control channel
}

int createSocket(bool lstn, int port)
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

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (!lstn)
    {
        // connect
        int connection_status =
            connect(cSocket,
                    (struct sockaddr *)&addr,
                    sizeof(addr));

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
                 (struct sockaddr *)&addr,
                 sizeof(addr)) < 0)
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

        printf("Client has started and is listening on PORT [%d]\n", port);

        // FOR Server connections later in the code
        // Server socket address structures
        struct sockaddr_in svc_address;
        socklen_t addr_size = sizeof(svc_address);
        int dataSocket = accept(
            cSocket, (struct sockaddr *)&addr,
            &addr_size);
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

        if (login_state == -1 && strstr(command, "USER") && statusCode == "331")
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
        else if (login_state == 0 && strstr(command, "PASS") && statusCode == "230")
        {
            login_state++;
        }
        else if (login_state == 1)
        {
            if (strstr(command, "CWD") && statusCode == "200")
            {
                printf("SERVER RESPONSE FOR CWD:\n%s", buffer);
            }
            else if (strstr(command, "PWD" && statusCode == "257"))
            {
                printf("SERVER RESPONSE FOR PWD:\n%s", buffer);
            }
            // Data Channel Commands
            else if (strstr(command, "LIST") && statusCode == "150")
            {
                printf("DATA CHANNEL: LIST");
                int channel = createSocket(true, C_PORT);
            }
            else if (strstr(command, "RETR") && statusCode == "150")
            {
                printf("DATA CHANNEL: RETR");
                int channel = createSocket(true, C_PORT);
            }
            else if (strstr(command, "STOR") && statusCode == "150")
            {

                // int channel = createSocket(false, C_PORT);
            }
        }
    }
    bzero(buffer, sizeof(buffer));
}

int main()
{
    int cSocket = createSocket(false, S_PORT);

    // accept command
    char buffer[BUFFER_SIZE];
    while (running)
    {
        // take command input
        printf("ftp> ");
        fgets(buffer, sizeof(buffer), stdin);

        // remove trailing newline char from buffer, fgets doesn't do it
        buffer[strcspn(buffer, "\n")] = 0; // review 0 or '0'

        handleCommands(buffer, cSocket);
    }

    return 0;
}
