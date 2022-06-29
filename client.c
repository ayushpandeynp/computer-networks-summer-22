#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#define S_PORT 9007 // Control Channel PORT
#define C_PORT 8080 // Data Channel PORT - should be random later

char buf[1023]; // buffer for receiving data - should make sense later

bool running = true;
bool dataTransferRunning = false;

int listenOnDataChannel()
{
	int cSocket;
	cSocket = socket(AF_INET, SOCK_STREAM, 0);

	// check for fail error
	if (cSocket == -1)
	{
		printf("Data channel creation has failed. \n");
		exit(EXIT_FAILURE);
	}

	// setsock
	int value = 1;
	// Reuse port after Service ends
	setsockopt(cln_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

	// define client address structure
	struct sockaddr_in cln_address;
	bzero(&cln_address, sizeof(cln_address));
	cln_address.sin_family = AF_INET;
	cln_address.sin_port = htons(C_PORT);
	cln_address.sin_addr.s_addr = INADDR_ANY;

	// FOR Server connections later in the code
	// Server socket address structures
	struct sockaddr_in svc_address;

	// Stores byte size of server socket address
	socklen_t addr_size;

	// bind the socket to our specified IP and port
	if (bind(cln_socket,
			 (struct sockaddr *)&cln_address,
			 sizeof(cln_address)) < 0)
	{
		printf("socket bind failed..\n");
		exit(EXIT_FAILURE);
	}

	// after it is bound, we can listen for connections with queue length of 5
	if (listen(cln_socket, 5) < 0)
	{
		printf("Listen failed..\n");
		close(cln_socket);
		exit(EXIT_FAILURE);
	}

	printf("Client has started and is listening on PORT [%d]\n", C_PORT);

	addr_size = sizeof(svc_address);
	int dataSocket = accept(
		cln_socket, (struct sockaddr *)&cln_address,
		&addr_size);

	return dataSocket;
}

void handleCommands(char buffer[], int cSocket)
{
	int login_state = -1;
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
		printf("CWD COMMAND ON MACHINE\n");
	}
	else if (strstr(command, "!PWD"))
	{
		printf("PWD COMMAND ON MACHINE\n");
	}
	else if (strstr(command, "!LIST"))
	{
		printf("LIST COMMAND ON MACHINE\n");
	}
	// Control Channel Commands
	else
	{
		// send command to server
		send(cSocket, buffer, strlen(buffer), 0);
		bzero(buffer, sizeof(buffer));
		recv(cSocket, buffer, sizeof(buffer), 0);

		// printing message from server
		printf("%s\n", buffer);

		char statusCode[4];
		strncpy(statusCode, buffer + 0, 3);

		if (login_state == -1 && strstr(command, "USER") && statusCode == "331")
		{
			// TO-DO:
			// char cwd[500];
			// if (getcwd(cwd, sizeof(cwd)) != NULL)
			// {
			// 	printf("Current working dir: %s\n", cwd);
			// }
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
			}
			else if (strstr(command, "RETR") && statusCode == "150")
			{
				printf("DATA CHANNEL: RETR");
			}
			else if (strstr(command, "STOR") && statusCode == "150")
			{
				printf("DATA CHANNEL: STOR");
			}
		}
	}
	bzero(buffer, sizeof(buffer));
}

int controlSocket()
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

	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(S_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	// connect
	int connection_status =
		connect(cSocket,
				(struct sockaddr *)&server_address,
				sizeof(server_address));

	// check for errors with the connection
	if (connection_status == -1)
	{
		printf("There was an error making a connection to the remote socket. \n\n");
		exit(EXIT_FAILURE);
	}

	return cSocket;
}
int main()
{
	int cSocket = controlSocket();

	// accept command
	char buffer[256];
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
