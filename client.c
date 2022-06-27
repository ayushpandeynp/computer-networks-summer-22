#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h> 	


#include<unistd.h>
#include<stdlib.h>

#define PORT 9007 //or 8080 or any other unused port value
#define CPORT 8080
char buf[1023];

int dataconn(){
	int cln_socket;
	cln_socket = socket(AF_INET , SOCK_STREAM,0);

	//check for fail error
	if (cln_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }

	//setsock
	int value  = 1;

	//Reuse port after Service ends
	setsockopt(cln_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); 
	
	//define client address structure
	struct sockaddr_in cln_address;
	bzero(&cln_address,sizeof(cln_address));
	cln_address.sin_family = AF_INET;
	cln_address.sin_port = htons(CPORT);
	cln_address.sin_addr.s_addr = INADDR_ANY;


	//FOR Server connections later in the code
	// Server socket address structures
	struct sockaddr_in svc_address;

	// Stores byte size of server socket address
	socklen_t addr_size;
	/////////////////////////////

	//bind the socket to our specified IP and port
	if (bind(cln_socket , 
		(struct sockaddr *) &cln_address,
		sizeof(cln_address)) < 0)
	{
		printf("socket bind failed..\n");
        exit(EXIT_FAILURE);
	}
	 
	//after it is bound, we can listen for connections with queue length of 5
	if(listen(cln_socket,5)<0){
		printf("Listen failed..\n");
		close(cln_socket);
        exit(EXIT_FAILURE);
	}
	

	printf("Client started on and is listening on PORT [%d]\n", CPORT);
	
	addr_size = sizeof(svc_address);
	
	int server_socket = accept(
			cln_socket, (struct sockaddr*)&cln_address,
			&addr_size);	

	return server_socket;
}

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
	setsockopt(network_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	printf("%d", INADDR_ANY);

	
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
	        	send(network_socket,buffer,strlen(buffer), 0);
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
			printf("%s \n", buffer);
			char cwd[500];
		   if (getcwd(cwd, sizeof(cwd)) != NULL) {
		       printf("Current working dir: %s\n", cwd);
		   }
			if(strstr(buffer,"STOR") != NULL)
	        {
	        	send(network_socket,buffer,strlen(buffer),0);
	        	int datafd = dataconn();

	        	char* filename = buffer + 5;
	        	//printf("FN: %s\n",filename);

	        	FILE *fp; fp = fopen(filename, "rb"); 
	           	//char *temp, *filename = buffer+5;
				
				if(fp == NULL){
					printf("can't open %s\n", filename);
				}
				printf("Sending STOR Request for %s.\n", filename);
				write(network_socket, buf, sizeof(buf));
				struct stat stat_buf;
		    	int rc = stat(filename, &stat_buf);
		    	int fsize = stat_buf.st_size;
		    	printf("%s size is %d bytes.\n", filename, fsize);
				char *databuff[fsize+1];
				fread(databuff,1,sizeof(databuff),fp);
				int total=0, bytesleft=fsize, ln;
				printf("Sending %s......\n", filename);

				while(total<fsize){
					printf("%ld \n", sizeof(databuff));
					ln = send(datafd, databuff+total, bytesleft, 0);
					printf("%ld \n",sizeof(ln));
					if(ln==-1)	break;
					total+=ln;
					bytesleft-=ln;
				}
				printf("Total data sent for %s = %d bytes.\n", filename, total);
				bzero(databuff, sizeof(databuff));
				fclose(fp); 
	        	

	        	bzero(buffer,sizeof(buffer));
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
