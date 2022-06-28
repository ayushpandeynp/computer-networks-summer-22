#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#define PORT 9007  // Control Channel PORT
#define CPORT 8080 // Data Channel PORT - should be random later

char buf[1023]; // buffer for receiving data - should be random later
bool running = true;

int dataconn()
{
	int cln_socket;
	cln_socket = socket(AF_INET, SOCK_STREAM, 0);

	// check for fail error
	if (cln_socket == -1)
	{
		printf("socket creation failed..\n");
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
	cln_address.sin_port = htons(CPORT);
	cln_address.sin_addr.s_addr = INADDR_ANY;

	// FOR Server connections later in the code
	//  Server socket address structures
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

	printf("Client started on and is listening on PORT [%d]\n", CPORT);

	addr_size = sizeof(svc_address);

	int server_socket = accept(
		cln_socket, (struct sockaddr *)&cln_address,
		&addr_size);

	return server_socket;
}

void handleCommands(char buffer[], int cSocket)
{
	int login_state = -1;
	char command[5];
	strncpy(command, buffer + 0, 4);

	// QUIT Control Command Handling
	if (strstr(buffer, "QUIT"))
	{
		printf("221 Service closing control connection. \n");
		// close(*cSocket);
		running = false;
		return;
	}

	send(cSocket, buffer, strlen(buffer), 0);
	bzero(buffer, sizeof(buffer));
	recv(cSocket, buffer, sizeof(buffer), 0);

	// printing message from server
	printf("%s\n", buffer);

	char statusCode[4];
	strncpy(statusCode, buffer + 0, 3);

	if (login_state == -1)
	{
		if (strstr(command, "USER") && statusCode == "331")
		{
			login_state++;
		}
		else if (login_state == 0 && strstr(command, "PASS") && statusCode == "230")
		{
			login_state++;
		}
		else
		{
			// printf("%s \n", buffer);
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

			// if (strstr(buffer, "RETR") == 0)
			// {
			// }

			// if (strstr(buffer, "LIST") == 0)
			// {
			// }

			// if (strstr(buffer, "CWD") == 0)
			// {
			// }

			// if (strstr(buffer, "PWD") == 0)
			// {
			// }

			// if (strstr(buffer, "!CWD") == 0)
			// {
			// }

			// if (strstr(buffer, "!PWD") == 0)
			// {
			// }

			// if (strstr(buffer, "!LIST") == 0)
			// {
			// }
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
	int value = 1;														  // scope is closed only until next line
	setsockopt(cSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	// connect
	int connection_status =
		connect(cSocket,
				(struct sockaddr *)&server_address,
				sizeof(server_address));

	// check for errors with the connection
	if (connection_status == -1)
	{
		printf("There was an error making a connection to the remote socket \n\n");
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
		printf("FTP-> ");
		fgets(buffer, sizeof(buffer), stdin);

		// remove trailing newline char from buffer, fgets does not remove it
		// buffer[strcspn(buffer, "\n")] = 0; // review 0 or '0'
		// send(cSocket, buffer, strlen(buffer), 0);
		handleCommands(buffer, cSocket);
	}

	return 0;
}