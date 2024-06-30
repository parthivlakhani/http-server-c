#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#define PORT 4221

void handle_connection(int fd, char *directory);

int main(int argc, char **argv) { // argc - stores number of command line arguments, argv - array of character pointers listing all arguments  

	char *directory = "."; // represents the current directory
	if (argc >= 3) // checks for command line arguments 
	{
		if (strcmp(argv[1], "--directory") == 0) 
		{
			directory = argv[2];
		}
	}
	printf("Setting up directory to %s\n", directory);

	if (chdir(directory) < 0) // chnaging the current working directory (if chdir fails returns value less than 0)
	{
		printf("Failed to set current dir");
		return 1;
	}

	/*
	 * Disable output buffering.
	 * This means that the program will not wait to
	 * output to the terminal
	 *
	 * For example, when doing
	 * printf("a");
	 * printf("b");
	 * printf("c");
	 * The program will output a,b, and c immediately after
	 * each print instead of waiting for a newline ('\n')
	 */
	setbuf(stdout, NULL);

	printf("Program Starts: \n");

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	struct sockaddr_in serv_addr = { 
			.sin_family = AF_INET , // Set the address family to AF_INET which is used for IPv4
			.sin_port = htons(PORT), // htons() host to network short  
			.sin_addr = { htonl(INADDR_ANY) }, // htonl() host to network long
	};

	server_fd = socket(AF_INET, SOCK_STREAM, 0); // creating socket for server
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
	
	// Bind the server socket to the specified address and port
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5; // maximum length of the queue of pending connection

	// listen() indicates that server is ready to accept incoming connection 
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	while(1){
		printf("Waiting for a client to connect...\n");

		client_addr_len = sizeof(client_addr);
		
		// accept connection from client and get a new file descriptor for it
		int fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		printf("Client connected\n");

		// For handling multiple client connection concurrently by for new child process to each client connection
		// create child process to handle client connection
		if (!fork())
		{
			close(server_fd); // close the server socket in clild process 
			handle_connection(fd,directory); // handle the client connection
			close(fd); // close the client connection
			exit(0); // exit child process
		}
		// close the client socket as it is handled by child process
		close(fd);
	}
	return 0;
}

void handle_connection(int fd, char *directory){

	char req_buffer[4096]; // hold incoming request
	int bytesReceived = recv(fd, req_buffer, sizeof(req_buffer), 0); // receives data on the fd socket and store in req_buffer 
	
	if (bytesReceived == -1) {
    	printf("Read failed: %s \n", strerror(errno));
    	return;
  	}	
	
	// strdup() - duplicate a string
	char *method = strdup(req_buffer);  // "GET /some/path HTTP/1.1..."
	char *content = strdup(req_buffer); // "GET /some/path HTTP/1.1..."
	printf("Content: %s\n", content);
	method = strtok(method, " "); // store the method - GET POST etc.

	// Extract the path -> "GET /some/path HTTP/1.1..."
	char *reqpath = strtok(req_buffer, " "); // -> "GET"
	reqpath = strtok(NULL, " "); // -> "/some/path"

	// send() sends data on the fd socket.
	// strncmp - compares at most the first n bytes of str1 and str2.
	// strcmp() compare two strings and return 0 if both strings are same, 
	// >0 if (string1's first character's ASCII - string2's first character's ASCII) > 0
	// <0 if (string1's first character's ASCII - string2's first character's ASCII) < 0
	if(strcmp(reqpath, "/")==0)
	{
		char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nWelcome to the server!\n";
		send(fd, response, strlen(response),0);
	}
	else if(strncmp(reqpath,"/echo/", 6)==0) 
	{
		reqpath = strtok(reqpath, "/"); // reqpath -> echo
		reqpath = strtok(NULL,""); // reqpath -> hello/world
		int len=strlen(reqpath);

		char response[1024]; 
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", len,reqpath);
		send(fd,response, strlen(response),0);
	}
	else if(strcmp(reqpath,"/user-agent")==0){

		reqpath = strtok(NULL, "\r\n"); // reqpath -> HTTP/1.1
		reqpath = strtok(NULL, "\r\n"); // reqpath -> Host: 127.0.1:4221
		reqpath = strtok(NULL, "\r\n"); // reqpath -> USer-Agent: curl/7.81.0

		char *body = strtok(reqpath, " "); // body -> User-Agent:
		body = strtok(NULL, " "); //body-> curl/7.81.0
		int len = strlen(body);

		char response[1024];
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", len,body);
		send(fd, response, strlen(response),0);
	}
	else if(strncmp(reqpath, "/files/", 7)==0 && strcmp(method, "GET")==0){
		
		char *filename = strtok(reqpath, "/");
		filename = strtok(NULL, "");

		// Open the file and check if the file exists
		FILE *fp = fopen(filename, "rb");
		if (!fp)
		{
			// If it doesn't exist, return 404
			printf("File not found");	
			char *res = "HTTP/1.1 404 Not Found\r\n\r\n"; 
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
    char *filename = strtok(reqpath, "/");
    filename = strtok(NULL, "");

    // Find Content-Length header
    char *content_length_str = strstr(content, "Content-Length: ");
    if (content_length_str == NULL) {
        printf("Could not find Content-Length header\n");
        char *res = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(fd, res, strlen(res), 0);
        return;
    }
    content_length_str += 16; // Skip "Content-Length: "
    int content_length = atoi(content_length_str);

    // Find the start of the request body
    char *body = strstr(content, "\r\n\r\n");
    if (body == NULL) {
        printf("Could not find request body\n");
        char *res = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(fd, res, strlen(res), 0);
        return;
    }
    body += 4; // Skip "\r\n\r\n"

    // Construct the full file path
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", directory, filename);
    printf("Attempting to create file: %s\n", filepath);

    // Open the file for writing
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        printf("Could not open file for writing: %s\n", strerror(errno));
        char *res = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        send(fd, res, strlen(res), 0);
        return;
    }

    printf("File opened successfully\n");

    // Write the contents
    size_t written = fwrite(body, 1, content_length, fp);
    if (written != content_length) {
        printf("Error writing file contents: %s (wrote %zu of %d bytes)\n", strerror(errno), written, content_length);
        fclose(fp);
        char *res = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        send(fd, res, strlen(res), 0);
        return;
    }

    printf("File contents written successfully\n");

    fclose(fp);

   // Send 201 Created response
    char *response = "HTTP/1.1 201 Created\r\n\r\n";
    send(fd, response, strlen(response), 0);
    
    // Add this line to print the response for debugging
    printf("Sent response: %s\n", response);
}
	else{
		char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
		send(fd,response, strlen(response),0);
	}
	return;
}