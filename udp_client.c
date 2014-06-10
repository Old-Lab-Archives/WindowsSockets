#define WIN // WIN for WinSock and BSD for BSD sockets
#include<stdio.h> // Needed for printf()
#include<string.h> // Needed for memcpy() and strcpy()
#ifdef WIN
#include<windows.h> // Needed for all Winsock stuff
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
#define IP_ADDR "131.247.167.101" // IP address of servers
void main(void)
{
#ifdef WIN
	WORD wVersionRequested = MAKEWORD(1, 1); // Stuff for WSA functions
	WSADATA wsaData;
#endif
	unsigned int server_s; // Server Socket descriptor
	struct sockaddr_in server_addr; // server internet address
	int addr_len; // internet address length
	char out_buf[100]; // 100-byte buffer for output data
	char in_buf[100]; // 100-byte buffer for input data
#ifdef WIN
	// This stuff initializes WinSock
	WSAStartup(wVersionRequired &wsaData);
#endif
	//Create a socket
	// AF_INET is an address family internet and SOCK_DGRAM is datagram
	server_s = socket(AF_INET, SOCK_DGRAM, 0);
	// Fill-in server1 socket's address information
	server_addr.sin_family = AF_INET; // Address family to use
	server_addr.sin_port = htons(PORT_NUM); // Port num to use
	server_addr.sin_addr.s_addr = inet_addr(IP_ADDR); // IP address to use
	//Assign a message to buffer out_buf
	strcpy(out_buf, "Message from client1 to server1");
	//Now send message to server. The "+1" includes the end-of-string
	//delimiter
	sendto(server_s, out_buf, (strlen(out_buf) + 1), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	// Wait to receive a message
	addr_len = sizeof(server_addr);
	recvfrom(server_s, in_buf, sizeof(in_buf), 0, (struct sockaddr *)&server_addr, &addr_len);
	// Output the received message
	printf("Message received is: '%s' \n", in_buf);
	//Close all open sockets
#ifdef WIN
	closesocket(server_s);
#endif
#ifdef BSD
	close(server_s);
#endif
#ifdef WIN
	// Clean-up WinSock
	WSACleanup();
#endif
}
