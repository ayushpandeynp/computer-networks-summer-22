#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h> 

#define PORT 9007
#define CPORT 8080
#define MAX 64

int arr_size;
char buf[MAX];

// Object to load login details
typedef struct
{
	char *user;
	char *pass;
} login_details;

// Array that stores the login data
login_details data[100];

// load function which reads the file from the directory and loads data into the array
int load(char *filename)
{
	char buffer[200];
	char token[50];
	login_details *log;
	FILE *file = fopen(filename, "r");
	int size = 0;
	char delim[] = ",";
	int x = 2;
	int k = 0;
	int i;
	char format_str[50] = "\0";
	while (fgets(buffer, 200, file) != NULL)
	{
		log = (login_details *)malloc(sizeof(login_details));
		char *ptr = strtok(buffer, delim);
		while (ptr != NULL)
		{
			// printf("%s", ptr);
			if (x % 2 == 0)
			{
				log->user = strdup(ptr);
			}
			else if (x % 2 == 1)
			{
				for (i = 0; i < strlen(ptr); i++)
				{
					k = ptr[i];
					if (k > 31 && k < 127)
					{
						char c = k;
						strcat(format_str, &c);
					}
					format_str[i + 1] = '\0';
				}
				format_str[i] = '\0';
				log->pass = strdup(format_str);
				bzero(format_str, sizeof(format_str));
			}
			ptr = strtok(NULL, delim);
			x++;
		}
		data[size] = *log;
		size++;
	}
	fclose(file);
	return size;
}
//-------------------------------------------------------------------------------------------------------

int tempconnect()
{
	int network_socket;
	network_socket = socket(AF_INET, SOCK_STREAM, 0);

	// check for fail error
	if (network_socket == -1)
	{
		printf("socket creation failed..\n");
		exit(EXIT_FAILURE);
	}

	// setsock
	int value = 1;
	setsockopt(network_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(CPORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	printf("%d", INADDR_ANY);

	// connect
	int connection_status =
		connect(network_socket,
				(struct sockaddr *)&server_address,
				sizeof(server_address));

	// check for errors with the connection
	if (connection_status == -1)
	{
		printf("There was an error making a connection to the remote socket \n\n");
		exit(EXIT_FAILURE);
	}

	return network_socket;
}
//-----------------------------------------------------------------------------------------------------------------
void handleCommands(int fd, char buffer[], int* login_state){
	
	printf("INITIAL: %d\n",*login_state);
	//char buffer[256]; buffer = buff;
	char command[6]; char check_username[256];
	bzero(command, sizeof(command));
	strncpy(command, buffer + 0, 5);

	char stsCode1[] = "530 Not logged in";
	char stsCode2[] = "331 Username OK, need password";
	char stsCode3[] = "230 User logged in, please proceed ...";
	char stsCode4[] = "503 Bad sequence of commands";
	char stsCode5[] = "202 Command not implemented";
	char stsCode6[] = "257 ";

	//Control Channel Commands after User Authentication
	if (strstr(command, "CWD") != NULL && (*login_state == 1))
	{
		chdir("server_dir");
		char ch_dir[500]; strncpy(ch_dir, buffer + 5, sizeof(buffer));
		printf("dir: %s \n", ch_dir);
		if(chdir(ch_dir) != 0){
			perror("No such directory");
		}
	}
	else if (strstr(command, "PWD") != NULL && (*login_state == 1))
	{
		chdir("server_dir");
		char cwd[500];
    	if (getcwd(cwd, sizeof(cwd)) != NULL) {
    		strcat(stsCode6, cwd);
    		printf("%s\n",stsCode6);
       		send(fd, stsCode6, sizeof(stsCode6), 0);
   		}
	}

	//Data Channel Commands after User Authentication
	else if (strstr(command, "STOR") != NULL && (*login_state == 1))
	{
		int cln_conn = tempconnect();
		char *name = command + 16;
		printf("Received PUT request for %s.\n", name);
		FILE *fp;
		fp = fopen(name, "wb");
		char *buff[MAX];
		bzero(buf, MAX);
		int total = 0, ln;
		while ((ln = read(cln_conn, buf, MAX)) > 0)
		{
			fwrite(buf, 1, sizeof(buf), fp);
			total += ln;
			if (ln < MAX)
				break;
		}
		printf("Bytes of data received for %s = %d.\n", name, total);
		fclose(fp);
	}
	else if(strstr(command, "RETR") != NULL && (*login_state == 1))
	{

	}
	else if(strstr(command, "LIST") != NULL && (*login_state == 1))
	{

	}

	//USER Authentication
	else{
		
		if (strstr(command, "USER"))
		{
			char *username = buffer + 5;
			printf("%s\n",username);
			printf("HERE: %d\n",*login_state );
			for (int i = 0; i < arr_size - 1; i++)
			{
				if (strcmp(username, data[i].user) == 0)
				{
					printf("TRUEEEE\n");
					(*login_state)++;
					send(fd, stsCode2, sizeof(stsCode2), 0);
					bzero(check_username, sizeof(check_username));
					memcpy(check_username, username, strlen(username));
					break;
				}
			}

			printf("logn: %d\n",*login_state );
			if (*login_state == -1)
			{
				send(fd, stsCode1, sizeof(stsCode1), 0);
			}
		}
		else if (strstr(command, "PASS") != NULL)
		{
			char *password = buffer + 5;
			if(((*login_state) == 0)){
				for (int i = 0; i < arr_size - 1; i++)
				{
					if (strcmp(password, data[i].pass) == 0 && strcmp(check_username, data[i].user)==0)
					{
						printf("MATCH\n");
						(*login_state)++;
						send(fd, stsCode3, sizeof(stsCode3), 0);
						break;
					}
				}if(*login_state == 0){*login_state--; send(fd, stsCode1, sizeof(stsCode1), 0);}
			}else if(*login_state == -1){
				send(fd, stsCode1, sizeof(stsCode1), 0);
			}else{
				printf("%d\n",*login_state);
				send(fd, stsCode4, sizeof(stsCode4), 0);
			}
		}
		else
		{
			send(fd, stsCode5, sizeof(stsCode5), 0); 
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------
int main()
{
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	printf("Server fd = %d \n", server_socket);

	// check for fail error
	if (server_socket < 0)
	{
		perror("socket:");
		exit(EXIT_FAILURE);
	}

	// setsock
	int value = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)); //&(int){1},sizeof(int)

	// define server address structure
	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	// bind
	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// listen
	if (listen(server_socket, 5) < 0)
	{
		perror("listen failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	// DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
	fd_set all_sockets;
	fd_set ready_sockets;

	// zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	// adds one socket (the current socket) to the fd set of all sockets
	FD_SET(server_socket, &all_sockets);

	printf("Server is listening...\n");

	// Flags

	char *filename = "users.txt";
	arr_size = load(filename);

	// Temporary Function to print users
	for (int i = 0; i < arr_size - 1; i++)
	{
		printf("%d %s : %s \n", i, data[i].user, data[i].pass);
	}

	int login_state = -1;
	while (1)
	{
		// notice so far, we have created 2 fd_sets : all_sockets , ready_sockets
		// but we have only used the all_sockets and didn't touch the ready_sockets
		// that is because select() is destructive: it's going to change the set we pass in
		// so we need a temporary copy; that is what the other fd_set ready_sockets is for

		// so that is why each iteration of the loop, we copy the all_sockets set into that temp fd_set
		ready_sockets = all_sockets;

		// now call select()
		// 1st argument: range of file descriptors to check  [the highest file descriptor plus one]
		// The maximum number of sockets supported by select() has an upper limit, represented by FD_SETSIZE (typically 1024).
		// you can use any number of max connections depending on your context/requirements

		// 2nd argument: set of file descriptors to check for reading (the ones we want select to keep an eye on)
		// 3rd argument: set of file descriptors to check for writing (usually NULL)
		// 4th argument: set of file descriptors to check for errors/exceptions (usually NULL)
		// 5th argument: optional timeout value specifying the time to wait for select to compplete
		if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
		{
			perror("select error");
			exit(EXIT_FAILURE);
		}

		// when select returns, we know that one of our file descriptors has work for us to do
		// but which one??
		// select returns the fd_set containing JUST the file descriptors ready for reading
		//(because select is destructive, so that is why we made the temp fd_set ready_sockets copy because we didn't want to lose the original set of file descriptors that we are watching)

		// to know which ones are ready, we have to loop through and check
		// go from 0 to FD_SETSIZE (the largest number of file descriptors that we can store in an fd_set)
		for (int fd = 0; fd < FD_SETSIZE; fd++)
		{
			// check to see if that fd is SET
			if (FD_ISSET(fd, &ready_sockets))
			{
				// if it is set, that means that fd has data that we can read right now
				// when this happens, we are interested in TWO CASES

				// 1st case: the fd is our server socket
				// that means it is telling us there is a NEW CONNECTION that we can accept
				if (fd == server_socket)
				{
					// accept that new connection
					int client_sd = accept(server_socket, 0, 0);
					printf("Client Connected fd = %d \n", client_sd);

					// add the newly accepted socket to the set of all sockets that we are watching
					FD_SET(client_sd, &all_sockets);
				}

				// 2nd case: when the socket that is ready to read from is one from the all_sockets fd_set
				// in this case, we just want to read its data
				else
				{
					//int login_state = -1; //NOT WORKING
					char buffer[256];
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
					handleCommands(fd, buffer, &login_state);
					
				}
			}
		}
	}

	// close
	close(server_socket);
	return 0;
}
