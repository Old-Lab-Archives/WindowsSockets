#ifdef WIN
#include<stdio.h> /* fprintf() */
#include<winsock.h> /* For WSAGetLastError() */
#include<stdlib.h> /* for exit() */
#endif
#ifdef BSD
#include<stdio.h> /*for fprintf() and perror() */
#include<stdlib.h> /*for exit()*/
#endif
void catch_error(char *program_msg)
{
	char err_descr[128]; /* for error description */
	int err;
	err = WSAGetLastError();
	/* winsock.h error description */
	if(err == WSANO_DATA)
		strcpy(err_descr, "WSANO_DATA (11004) Valid name, no data record of requested type");
	if(err == WSANO_RECOVERY)
		strcpy(err_descr, "WSANO_RECOVERY (11003). This is non-recoverable error");
	if(err == WSATRY_AGAIN)
		strcpy(err_descr, " Try again");
	fprintf(stderr,"%s: %s\n", program_msg, err_descr);
	exit(1);
}
