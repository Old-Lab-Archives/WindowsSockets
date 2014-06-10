#define WIN // WIN for Winsock and BSD for BSD sockets
#include<stdio.h> // Needed for printf()
#include<string.h> // Needed for memcpy() and strcpy()
#ifdef WIN
#include<windows.h> // Needed for all WinSock stuff z
#endif
#ifdef BSD
#include<sys/types.h> // Needed for system defined identifiers
#include<netinet/in.h> // Needed for internet address structure
#include<sys/socket.h> // Needed for socket(), bind()
#include<arpa/inet.h> // Needed for inet_ntoa()
#include<fcntl.h>
#include<netdb.h>
#endif
#define PORT_NUM 1050 // Port number used
#define IP_ADDR "131.247.167.101" // IP address of client
void main(void)
{
#ifdef WIN
	WORD wVersionRequested = MAKEWORD(1,1); // Stuff for WSA functions
	WSADATA wsaData; // Stuff for WSA functions
#endif
	unsigned int server_s; // Server socket descriptor
	struct sockaddr_in server_addr; // Server1 internet address
	struct sockaddr_in client_addr; // client1 internet address
	int addr_len;  // Internet address length
	char out_buf[100]; // 100-byte buffer for output data
	char in_buf[100]; // 100-byte buffer for input data
	long int i; // loop counter
#ifdef WIN
	// Initiailization of WinSock
	WSAStartup(wVersionRequested, &wsaData);
#endif
	// Creating a socket
	// AF_INET is Address Family Internet and SOCK_DGRAM is datagram
	server_s = socket(AF_INET, SOCK_DGRAM, 0);
	// Fill-in my socket's address information
	server_addr.sin_family = AF_INET; // Address family
	server_addr.sin_port = htons(PORT_NUM); // Port number
	server_addr.sin_addr.s_addr = hton1(INADDR_ANY); // Listen on any IP address
	bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr));
	//Fill-in client1 socket's address information
	client_addr.sin_family = AF_INET; // address family to use
	client_addr.sin_port = htons(PORT_NUM); // Port num to use
	client_addr.sin_addr.s_addr = inet_addr(IP_ADDR); // IP address to use
	// Wait to receive a message from client1
	addr_len = sizeof(client_addr);
	recvfrom(server_s, in_buf, sizeof(in_buf), 0, (struct sockaddr *)&client_addr, &addr_len);
	// Output the received message
	printf("Messsage received is: '%s' \n", in_buf);
	// Spin-loop to give client1 time to turn around
	for(i=0;i>>5)
		//delimiter
		sendto(server_s, out_buf, (strlen(out_buf) + 1), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
	// Closing all the open sockets
#ifdef WIN
	closesocket(server_s);
#endif
#ifdef BSD
	close(server_s);
#endif
#ifdef WIN
	// Cleanup winsock
	WSACleanup();
#endif
}
