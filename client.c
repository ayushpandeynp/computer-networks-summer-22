#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>

#include<unistd.h>
#include<stdlib.h>

#define PORT 1024 //or 8080 or any other unused port value


int main()
{
	//create a socket
	int network_socket;
	network_socket = socket(AF_INET , SOCK_STREAM, 0);

	//check for fail error
	if (network_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }
	
	//setsock
	int value  = 1;

	//Reuse Port after service ends
	setsockopt(network_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	
	//connect
	int connection_status = 
	connect(network_socket, 
			(struct sockaddr *) &server_address,
			sizeof(server_address));

	//check for errors with the connection
	if(connection_status == -1){
		printf("There was an error making a connection to the remote socket \n\n");
		exit(EXIT_FAILURE);
	}
	
	//accept
	char buffer[256]; char password[64]; char temp[256];
	temp[256] = '\0';
	int login = 0;
	int* test;
	while(1)
	{
       printf("FTP-> "); fgets(buffer,sizeof(buffer),stdin);
       buffer[strcspn(buffer, "\n")] = 0;  //remove trailing newline char from buffer, fgets does not remove it
       
       if(strcmp(buffer,"QUIT")==0)
        {
        	printf("221 Service closing control connection. \n");
        	close(network_socket);
            break;
        }

       if(login == 0){ 
	       if(strstr(buffer,"USER") != NULL)
	        {	
	        	//printf("%s", buffer);
	        	write(network_socket,buffer,strlen(buffer));
	        	bzero(buffer,sizeof(buffer));
	        	// read(network_socket,test, sizeof(test));
	        	recv(network_socket, buffer , sizeof(buffer),0);
	        	if(strcmp(buffer,"TRUE")==0){//strcmp(buffer,"T")==0){
	        		printf("From Server: 331 Username OK, need password \n");
	        		printf("FTP-> "); fgets(password,sizeof(password),stdin); password[strcspn(password, "\n")] = 0;
	        		if(strstr(password,"PASS") != NULL){
		        		send(network_socket,password,strlen(password),0);
		        		bzero(password,sizeof(password));
		        		recv(network_socket,&password,sizeof(password),0);
		        		if(strcmp(password,"TRUE")==0){
		        			printf("From Server: 230 User logged in, please proceed...\n");
		        			login = 1;
		        		}else{printf("530 Not logged in \n");}
	        		}else{
	        			printf("530 Not logged in \n");
	        		}
	        	}else{
	        		printf("530 Not logged in \n");	
	        	}
	        }else{
	        	printf("530 Not logged in \n");
	        }
	    }
	    else if((strstr(buffer,"USER") != NULL || strstr(buffer,"PASS") != NULL) && login == 1){
	        	printf("503 Bad sequence of commands\n");
	    }
	    else{
		
			if(strstr(buffer,"STOR")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"RETR")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"LIST")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"CWD")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"PWD")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"!CWD")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"!PWD")==0)
	        {
	        	
	        }

	        if(strstr(buffer,"!LIST")==0)
	        {
	        	
	        }
	    }

	 
        
        bzero(buffer,sizeof(buffer));			
	}

	return 0;
}