#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>

#include<unistd.h>
#include<stdlib.h>

#define PORT 1024 //or 8080 or any other unused port value

typedef struct {
    char* user;
    char* pass;
} login_details;

login_details data[100];

int load(char* filename)
{
    char buffer[200];
    char token[50];
    login_details* log;
    FILE* file = fopen(filename, "r");
    int size = 0;
    char delim[] = ",";	
    int x = 2; int k = 0; int i;
    char format_str[50] = "\0";
    while(fgets(buffer, 200, file) != NULL)
    {
    	log = (login_details*)malloc(sizeof(login_details));
        char *ptr = strtok(buffer, delim);
        while(ptr != NULL)
		{
			//printf("%s", ptr);
			if(x%2 == 0){
				log->user = strdup(ptr); 
			}else if (x%2 == 1){
				for (i = 0; i < strlen(ptr); i++)
				{
					k = ptr[i];
					if (k>31 && k<127)
					{
						char c = k;
						strcat(format_str, &c);
						//printf("%s - %d\n",format_str, strlen(format_str));
					}
					format_str[i+1]='\0'; 
				}
				format_str[i]='\0'; 
				//printf("TEST: %s-%d \n",format_str, strlen(format_str));
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

int main()
{	
	char* filename = "users.txt";

	int arr_size = load(filename);

   	for(int i=0; i<arr_size - 1;i++){
   		printf("%d %s : %s \n", i, data[i].user, data[i].pass);
   	}

	//Initialize string for Hostname
	char hostname[128];
	gethostname(hostname, 128);

	//create socket
	int server_socket;
	server_socket = socket(AF_INET , SOCK_STREAM,0);

	//check for fail error
	if (server_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }

	//setsock
	int value  = 1;

	//Reuse port after Service ends
	setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); 
	
	//define server address structure
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;


	//FOR client connections later in the code
	// Client socket address structures
	struct sockaddr_in client_address;

	// Stores byte size of server socket address
	socklen_t addr_size;
	/////////////////////////////

	//bind the socket to our specified IP and port
	if (bind(server_socket , 
		(struct sockaddr *) &server_address,
		sizeof(server_address)) < 0)
	{
		printf("socket bind failed..\n");
        exit(EXIT_FAILURE);
	}
	 
	//after it is bound, we can listen for connections with queue length of 5
	if(listen(server_socket,5)<0){
		printf("Listen failed..\n");
		close(server_socket);
        exit(EXIT_FAILURE);
	}
	

	printf("Server started on [%s] and is listening on PORT [%d]\n", hostname, PORT);
	
	addr_size = sizeof(client_address);
	//to track number of connected clients
	int count = 0;

	while(1)
	{
		//// Accept clients and
		// store their information in client_address
		int client_socket = accept(
			server_socket, (struct sockaddr*)&client_address,
			&addr_size);

		if(client_socket == -1){
			perror("Error");
		}
		// Displaying information of
		// connected client
		printf("Connection accepted from %s:%d\n",
			inet_ntoa(client_address.sin_addr),
			ntohs(client_address.sin_port));

		// Print number of clients
		// connected till now
		printf("Clients connected: %d\n\n",
			++count);

		//Flags
		char flag[] = "TRUE";
		char flag_1[] = "FALSE";
		int login = 0;


		int pid = fork(); //fork a child process

		if(pid == 0)   //if it is the child process
		 {
		 	close(server_socket); //close the copy of server/master socket in child process
		 	char buffer[256];
			while(1)
			{
				bzero(buffer,sizeof(buffer));
				int bytes = recv(client_socket,buffer,sizeof(buffer),0);
				if(bytes==0)   //client has closed the connection
				{
					printf("connection closed from client side \n");
					close(client_socket);
					exit(1); // terminate client program
				}


				if(strstr(buffer,"USER") != NULL){
					char *username = buffer + 5;
					for(int i=0; i<arr_size - 1;i++){
						//printf("USERS: %s \n",data[i].user);
				   		if(strcmp(username, data[i].user) == 0){
				   			login = 1;
				   			send(client_socket,flag,sizeof(flag), 0);
				   			break;
				   		}else{login = 0;}
				   	}
				   	if(login == 0){send(client_socket,flag_1,sizeof(flag_1),0);}
				}

				if(strstr(buffer,"PASS") != NULL){
					char *password = buffer + 5;
					for(int i=0; i<arr_size - 1;i++){
				   		if(strcmp(password,data[i].pass) == 0){
				   			login = 1;
				   			send(client_socket,flag,sizeof(flag),0); 
				   			break;
				   		}
				   		else{login = 0;}				   		
				   	}
				   	if(login == 0){send(client_socket,flag_1,sizeof(flag_1),0);}
				}	
			}
		 }
		 else //if it is the parent process
		 {
		 	close(client_socket); //close the copy of client/secondary socket in parent process 
		 }
	}


	
	close(server_socket);

	return 0;
}