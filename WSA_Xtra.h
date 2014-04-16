#ifndef _WSA_XTRA_
#define _WSA_XTRA_
#include<winsock.h>
#undef IPROTO_GGP
#define IPROTO_GGP 3
#ifndef IPROTO_IGMP
#define IPROTO_IGMP 2
#endif
/*for avoid word size sensitivity*/
#ifndef WIN32
#define EXPORT__export
#else
#define EXPORT
#endif
#define MAXHOSTNAME 255 /*maximum length of DNS host name*/
#define MAXADDRSTR 16 /*maximum length of IP address string*/

#define SOCKADDR_LEN sizeof(struct asockaddr)

#ifndef MAKEWORD
#define MAKEWORD(1,h) ((WORD)(((BYTE) (1))|(((WORD)(BYTE)(h))<<0)))
#endif
#define WSA_MAJOR_VERSION 1
#define WSA_MINOR_VERSION 1
#define WSA_VERSION MAKEWORD(WSA_MAJOR_VERSION,WSA_MINOR_VERSION)
/*for UNIX compatibility*/
#define bcopy(s,d,n) _fmemcpy((d),(s),(n))
#define bcmp(s1,s2,n) _fmemcmp((s1),(s2),(n))
#define bzero(s,n) _fmemset((s),0,(n))
#define IP_TTL 4
#define MAX_TTL 225
/*ICMP types*/
#define ICMP_ECHOREPLY 0
#define ICMP_ECHOREQ 8
/*definition of ICMP header*/
typedef struct icmp_hdr
{
	u_char icmp_type; /*message type*/
	u_char icmp_code; /*sub-code type*/
	u_short icmp_cksum; /*checksum*/
	u_short icmp_id; /*identifier*/
	u_short icmp_seq; /*sequence number*/
	char icmp_data[1]; /*data*/
}
ICMP_HDR, *PICMPHDR, FAR *LPICMPHDR;
#define ICMP_HDR_LEN sizeof(ICMP_HDR)
/* IP header version 4 definition*/
#define IPVERSION 4
typedef struct ip_hdr
{
	u_char ip_h1; /*header length*/
	u_char ip_v; /*version*/
	u_char ip_tos; /*type of service*/
	short ip_len; /*total length*/
	u_short ip_id; /*identification*/
	short ip_off; /*fragment offset field*/
	u_char ip_ttl; /*time span (time to live)*/
	u_char ip_p; /*protocol*/
	u_char ip_cksum; /*checksum*/
	struct in_addr ip_src; /*source address*/
	struct in_addr ip_dst; /*destination address*/
}
IP_HDR, *PIP_HDR, *LPIP_HDR;
#define IP_HDR_LEN sizeof(IP_HDR)
#ifndef WSA_MULTICAST
#define DEERING_OFFSET 7 /*subtract this bias for NT options*/
#define IP_MULTICAST_IF 9 /*set/get IP multicast interface*/
#define IP_MULTICAST_TTL 10 /*set/get IP multicast TTL*/
#define IP_MULTICAST_LOOP 11 /*set/get IP multicast loopback */
#define IP_ADD_MEMBERSHIP 12 /*add(Set)IP group membership*/
#define IP_DROP_MEMBERSHIP 13 /*drop(set)IP group membership*/
#define IP_DEFAULT_MULTICAST_TTL 1
#define IP_DEFAULT_MULTICAST_LOOP 1
#define IP_MAX_MEMBERSHIPS 20
typedef struct ip_mreq
{
	struct in_addr imr_multiaddr; /*multicast group to join*/
	struct in_addr imr_interface; /*interface to join on*/
}
IP_MREQ;
#endif
#endif /*final end _WSA_XTRA_ */

