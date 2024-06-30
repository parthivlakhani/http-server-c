#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void handle_connection(int fd);

int main(int argc, char **argv) {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	char *directory = ".";
	if (argc >= 3)
	{
		if (strcmp(argv[1], "--directory") == 0)
		{
			directory = argv[2];
		}
	}
	printf("Setting up directory to %s\n", directory);

	if (chdir(directory) < 0)
	{
		printf("Failed to set current dir");
		return 1;
	}

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	//
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	while(1){
		printf("Waiting for a client to connect...\n");
		client_addr_len = sizeof(client_addr);
		
		int fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		printf("Client connected\n");

		if (!fork())
		{
			close(server_fd);
			handle_connection(fd);
			close(fd);
			exit(0);
		}
		close(fd);
	}
	

	return 0;
}

void handle_connection(int fd){
	char req_buffer[4096];
	int bytesReceived = recv(fd, req_buffer, sizeof(req_buffer), 0);
	
	if (bytesReceived == -1) {
    	printf("Read failed: %s \n", strerror(errno));
    	return;
  	}	
	req_buffer[bytesReceived] = '\0';
	
	char *method = strdup(req_buffer);
	char *content = strdup(req_buffer);
	printf("Content: %s\n", content);
	method = strtok(method, " ");
	char *reqpath = strtok(req_buffer, " ");
	reqpath = strtok(NULL, " ");

	int byteSent;

	if(strcmp(reqpath, "/")==0){
		char *response = "HTTP/1.1 200 OK\r\n\r\n";
		send(fd, response, strlen(response),0);
	}
	else if(strncmp(reqpath,"/echo/", 6)==0){

		reqpath = strtok(reqpath, "/");
		reqpath = strtok(NULL,"");
		int len=strlen(reqpath);
		char response[1024]; 
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", len,reqpath);
		send(fd,response, strlen(response),0);
	}
	else if(strcmp(reqpath,"/user-agent")==0){
		reqpath = strtok(NULL, "\r\n");
		reqpath = strtok(NULL, "\r\n");
		reqpath = strtok(NULL, "\r\n");

		char *body = strtok(reqpath, " ");
		body = strtok(NULL, " ");
		int len = strlen(body);

		char response[1024];
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", len,body);
		send(fd, response, strlen(response),0);
	}
	else if(strncmp(reqpath, "/files/", 7)==0 && strcmp(method, "GET")==0){
		
		// Parse the file path
		char *filename = strtok(reqpath, "/");
		filename = strtok(NULL, "");

		// Open the file and check if the file exists
		FILE *fp = fopen(filename, "rb");
		if (!fp)
		{
			// If it doesn't exist, return 404
			printf("File not found");	
			char *res = "HTTP/1.1 404 Not Found\r\n\r\n"; // HTTP response
			send(fd, res, strlen(res), 0);
		}
		else
		{
			printf("Opening file %s\n", filename);
		}

		// Read in binary and set the cursor at the end
		if (fseek(fp, 0, SEEK_END) < 0)
		{
			printf("Error reading the document\n");
		}

		// Get the size of the file
		size_t data_size = ftell(fp);

		// Rewind the cursor back
		rewind(fp);

		// Allocate enough memory for the contents
		void *data = malloc(data_size);

		// Fill in the content
		if (fread(data, 1, data_size, fp) != data_size)
		{
			printf("Error reading the document\n");
		}

		fclose(fp);

		// Return contents
		char response[1024];
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n\r\n%s", data_size, (char *)data);
		printf("Sending response: %s\n", response);
		send(fd, response, strlen(response), 0);
	}
	else if (strncmp(reqpath, "/files/", 7) == 0 && strcmp(method, "POST") == 0)
	 {
		method = strtok(NULL, "\r\n"); // HTTP 1.1
		method = strtok(NULL, "\r\n"); // Content-Type
		method = strtok(NULL, "\r\n"); // User-Agent
		method = strtok(NULL, "\r\n"); // Accept: */*
		method = strtok(NULL, "\r\n"); // Content-Length: X

		char *contentLengthStr = strtok(method, " ");
		contentLengthStr = strtok(NULL, " ");

		int contentLength = atoi(contentLengthStr);

		// Parse the file path
		char *filename = strtok(reqpath, "/");
		filename = strtok(NULL, "");

		// Get the contents
		content = strtok(content, "\r\n"); // Content: POST /files/dumpty_yikes_dooby_237 HTTP/1.1
		printf("\n\n\nContent: %s\n\n\n", content);
		content = strtok(NULL, "\r\n"); // Host: localhost:4221
		content = strtok(NULL, "\r\n"); // User-Agent: curl/7.81.0
		content = strtok(NULL, "\r\n"); // Accept: */*
		content = strtok(NULL, "\r\n"); // Content-Length: 51
		printf("\n\n\nContent: %s\n\n\n", content);
		content = strtok(NULL, "\r\n"); // Content-Type: application/x-www-form-urlencoded
		printf("\n\n\nContent: %s\n\n\n", content);
		content = strtok(NULL, "\r\n"); // Content-Type: application/x-www-form-urlencoded
		printf("\n\n\nContent: %s\n\n\n", content);

		printf("\n---\nCreate a file %s with content length: %d\n\n %s\n---\n", filename, contentLength, content);

		// Open the file in write binary mode
		FILE *fp = fopen(filename, "wb");
		if (!fp)
		{
			// If the file could not be created/opened
			printf("File could not be opened");
			char *res = "HTTP/1.1 404 Not Found\r\n\r\n"; // HTTP response
			send(fd, res, strlen(res), 0);
		}
		else
		{
			printf("Opening file %s\n", filename);
		}

		// Write the contents
		if (fwrite(content, 1, contentLength, fp) != contentLength)
		{
			printf("Error writing the data");
		}

		fclose(fp);

		// Return contents
		// Return contents
		char response[1024];
		sprintf(response, "HTTP/1.1 201 Created\r\nContent-Type: application/octet-stream\r\nContent-Length: %d\r\n\r\n%s", contentLength, content);
		printf("Sending response: %s\n", response);
		send(fd, response, strlen(response), 0);
	}
	else{
		char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
		send(fd,response, strlen(response),0);
	}
	return;
}