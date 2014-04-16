/*main header*/

#define MAKEWORD(low, high) \ ((WORD)(((BYTE)(low))|(((WORD)(BYTE)(high))<<8)))
#define bcopy(s,d,n) _fmemcpy((d),(s),(n))
#define bcmp(s1,s2,n) _fmemcmp((s1),(s2),(n))
#define bzero(s,n) _fmemset((s),0,(n))
#define IN_CLASSD(i) (((long) (i) & 0Xf0000000)==0Xe00 /00000)
#define IN_MULTICAST(i) IN_CLASSD(i)
#define IP_HDRINCL 2 /* int; header is included with data */
#define IP_TOS 3 /* int; IP type of service and preced */
#define IP_TTL 4 /* int; IP time to live */
#define IP_RECVOPTS 5 /* bool; receive all IP opts w/dgram */
#define IP_RECVRETOPTS 6 /* bool; receive IP opts for response */
#define IP_RECVDSTADDR 7 /* bool; receive IP dst addr w/dgram */
#define IP_RETOPTS 8 /* ip_opts; set/get IP options */
#define IP_MULTICAST_IF 9 /* u_char; set/get IP multicast i/f */
#define IP_MULTICAST_TTL 10 /* u_char; set/get IP multicast ttl */
#define IP_MULTICAST_LOOP 11 /* u_char; set/get IP multicast loopback */
#define IP_ADD_MEMBERSHIP 12 /* ip_mreq; add an IP group membership */
#define IP_DROP_MEMBERSHIP 13 /* ip_mreq; drop an IP group membership */
#define IP_DEFAULT_MULTICAST_TTL 1 /* default TTL is one hop */
#define IP_DEFAULT_MULTICAST_LOOP 1 /* default loopback enabled */
#define IP_MAX_MEMBERSHIPS 20 /*maximum memberships per socket*/
#define INADDR_UNSPEC_GROUP (u_long)0Xe0000000
#define INADDR_ALLHOSTS_GROUP (u_long)0Xe0000001
#define AF_IPX 6 /*IPX and SPX*/
#define PF_IPX AF_IPX
#define SO_OPENTYPE 0X7008
#define SO_SYNCHRONOUS_ALERT 0X10
#define SO_SYNCHRONOUS_NONALERT 0X20
#define SO_CONNDATA 0X7000
#define SO_CONNOPT 0X7001
#define SO_DISCDATA 0X7002
#define SO_DISCOPT 0X7003
#define SO_CONNDATALEN 0X7004
#define SO_CONNOPTLEN 0X7005
#define SO_DISCDATALEN 0X7006
#define SO_DISCOPTLEN 0X7007
#define SO_MAXDG 0X7009
#define SO_MAXPATHDG 0X700A
#define MSG_PARTIAL 0X8000 /*partial send or recv for message export*/
#define WSAEDISCON (WSABASEERR+101)
#define SOCKET_ERROR -1
#define INVALID_SOCKET(SOCKET)(~0)
typedef u_int SOCKET;
setsockopt():
getsockopt():
#define SQL_SOCKET 0XFFFF
#define WSADESCRIPTION_LEN 256 /*max length of szDescription field*/
#define WSASYS_STATUS_LEN 128 /*max length of szSystemStatus field*/
FD_SETSIZE /*maximum number of sockets possible in a fdset */
	FD_CLR(fd,set) /*removes socket from fd_set */
	FD_SET(fd,set) /*adds a socket to fd_set*/
	FD_ZERO(set) /*clears the contents of fd_set*/
	FD_ISSET(fd,set) /*checks if socket is member of a set*/
	timerclear(tvp) /*sets timeout value to zero*/
	timerisset(tvp) /*boolean indicates if timeout value is non-zero*/
	timercmp(tvp, uvp, cmp) /*boolean indicates if timeout value tvp is greater or less than uvp*/
	FIONREAD /*get number of bytes available to read*/
	FIONBIO /*set/clear non-blocking operation mode*/
	FIOASYNC /*set/clear async I/O */
	SIOCSHIWAT /*set socket high-water mark*/
	SIOCGHIWAT /*get socket high-water mark*/
	SIOCSLOWAT /*set socket low-water mark*/
	SIOCGLOWAT /*get socket low-water mark*/
	SIOCATMARK /*boolean: FALSE if at OOB data mark (opposite of BSD) */
#define IPPROTO_IP 0 /*dummy for IP*/
	#define IPPROTO_ICMP 1 /*control message protocol*/
	#define IPPROTO_GGP 2 /*gateway^2 (deprecated)*/
	#define IPPROTO_TCP 6 /*tcp*/
	#define IPPROTO_PUP 12 /*pup*/
	#define IPPROTO_UDP 17 /* user datagram protocol */
	#define IPPROTO_IDP 22 /* xns idp */
	#define IPPROTO_ND 77 /*net disk proto*/
	#define IPPROTO_RAW 255 /* raw IP packet */
	#define IPPROTO_MAX 256 /*it is a single byte value*/
	#define IPPROTO_ECHO 7 /*TCP and UDP service echoes data sent*/
	#define IPPROTO_DISCARD 9 /*TCP and UDP service "eats" data sent*/
	#define IPPROTO_SYSTAT 11 /*TCP service returns system status*/
	#define IPPROTO_DAYTIME 13 /*TCP and UDP service returns formatted string*/
	#define IPPROTO_NETSTAT 15 /*TCP system net status services*/
	#define IPPROTO_FTP 21 /*TCP file transfer service*/
	#define IPPROTO_TELNET 23 /*TCP remote login service*/
	#define IPPROTO_SMTP 25 /*TCP simple mail transfer service*/
	#define IPPROTO_TIMESERVER 37 /*TCP and UDP time service*/
	#define IPPROTO_NAMESERVER 42
	#define IPPROTO_WHOIS 43 /*TCP and UDP user system info*/
	#define IPPROTO_MTP 57 /*TCP and UDP any private terminal access*/
	#define IPPROTO_TFTP 69 /*TCP and UDP trivial file transfer service */
	#define IPPROTO_RJE 77 /*TCP and UDP remote job entry*/
	#define IPPROTO_FINGER 79 /*TCP and UDP finger user and system info*/
	#define IPPROTO_TTYLINK 87 /*TCP and UDP any private terminal link*/
	#define IPPROTO_SUPDUP 95 /*TCP and UDP "supdup" application service*/
	#define IPPROTO_EXECSERVER 512 /*TCP remote execution*/
	#define IPPROTO_LOGINSERVER 513 /*TCP remote login*/
	#define IPPROTO_CMDSERVER 514 /*TCP shell service*/
#define IPPROTO_EFSSERVER 520 /*TCP extended file name server*/
	#define IPPROTO_BIFFUDP 512 /*UDP "biff" to notify user of mail*/
	#define IPPROTO_WHOSERVER 513 /*UDP remote Who's Who service*/
	#define IPPROTO_ROUTESERVER 520 /*UDP local routing process*/
#define IPPROTO_RESERVED 1024 /*last priveleged process*/
#define IMPLINK_IP 155
#define IMPLINK_LOWEXPER 156
#define IMPLINK_HIGHEXPER 158
	IN_CLASSA(i) (((long)(i) & 80000000)==0)
	IN_CLASSA_NET 0Xff000000
	IN_CLASSA_NSHIFT 24
	IN_CLASSA_HOST 0X00ffffff
	IN_CLASSA_MAX 128
	IN_CLASSB(i) (((long)(i) & 0Xc0000000)==0X80000000)
	IN_CLASSB_NET 0Xffff0000
	IN_CLASSB_NSHIFT 16
	IN_CLASSB_HOST 0X0000ffff
	IN_CLASSB_MAX 65536
	IN_CLASSC(i) (((long)(i) & 0Xc0000000)==0Xc0000000)
	IN_CLASSC_NET 0Xffffff00
	IN_CLASSC_NSHIFT 8
	IN_CLASSC_HOST 0X000000ff
#define INADDR_ANY (u_long)0X00000000 /*system default*/
#define INADDR_LOOPBACK 0X7f000001 /*loopback address 127.0.0.1 */
#define INADDR_BROADCAST (u_long)0Xffffffff /*limited broadcast addr*/
#define INADDR_NONE 0Xffffffff /*denotes invalid IP address*/
#define AF_UNSPEC 0 /*unspecified*/
#define AF_UNIX 1 /*local to host*/
#define AF_INET 2 /*internetwork: UDP, TCP, etc */
#define AF_IMPLINK 3 /*arpanet interface message processor*/
#define AF_PUP 4 /*pup protocols*/
#define AF_CHAOS 5 /*mit CHAOS protocols*/
#define AF_NS 6 /*XEROX NS protocols*/
#define AF_ISO 7 /*ISO protocols*/
#define AF_OSI AF_ISO /*OSI is ISO*/
	#define AF_ECMA 8 /*European computer manufacturers*/
	#define AF_DATAKIT 9 /*datakit protocols*/
	#define AF_CCITT 10 /*CCITT protocols, X.25, etc */
	#define AF_SNA 11 /*IBM SNA*/
	#define AF_DECnet 12 /*DECnet*/
	#define AF_DLI 13 /*Direct Data-line Interface*/
	#define AF_LAT 14 /*LAT*/
	#define AF_HYLINK 15 /*NSC Hyperchannel*/
#define AF_APPLETALK 16 /*AppleTalk*/
	#define AF_NETBIOS 17 /*Net-Bios style addresses*/
	#define AF_MAX 18
	#define SOCK_STREAM 1 /*stream socket*/
	#define SOCK_DGRAM 2 /*datagram socket*/
	#define SOCK_RAW 3 /*raw protocol interface*/
	#define SOCK_RDM 4 /* reliability delivered message*/
	#define SOCK_SEQPACKET 5 /*sequenced packet stream*/
	#define SOMAXCONN 5
	#define MSG_OOB 1 /*recv()/recvfrom():process urgent data*/
	#define MSG_PEEK 2 /*recv()/recvfrom():peek at incoming data*/
	#define MSG_DONTROUTE 4 /*send()/sendto():do not use routing tables*/
#define MAXGETHOSTSTRUCT 1024
	#define FD_READ 0X1 /*data availability*/
	#define FD_WRITE 0X2 /*write buffer availability*/
	#define FD_OOB 0X4 /*OOB data availability*/
	#define FD_ACCEPT 0X8 /*incoming connect request*/
	#define FD_CONNECT 0X10 /*outgoing connect completion*/
	#define FD_CLOSE 0X20 /*virtual circuit closed by remote*/
#define h_errno WSAGetLastError()
#define WSAGETASYNCBUFLEN(lParam) LOWORD(lParam)
	#define WSAGETASYNCERROR(lParam) HIWORD(lParam)
#define WSAGETSELECTEVENT(lParam) LOWORD(lParam)
#define WSAGETSELECTERROR(lParam) HIWORD(lParam)
