/* Asynchronous Signal-driven I/O */
#include<stdio.h>
#include<sys/signal.h>
#include<unistd.h>
void catch_error(char *errorMessage); /* for error handling */
void InterruptSignalHandler(int signalType); /* Handles interrupts, signals */
int main(int argc, char *argv[])
{
	struct sigaction handler; /*Signal handler specification*/
	/*--Set InterruptSignalHandler() as handler function--*/
	handler.sa_handler = InterruptSignalHandler;
	/*--Creating mask for all signals--*/
	if(sigfillset(&handler.sa_mask) < 0)
		catch_error("sigfillset() failed");
	/*No flags*/
	handler.sa_flags = 0;
	/*setting signal handler for interrupt signals*/
	if(sigaction(SIGINT, &handler, 0) < 0)
		catch_error("sigaction() failed");
	for(; ;)
		pause(); /*suspend program until signals received*/
	exit(0);
}
void InterruptSignalHandler(int signalType)
{
	printf("Interrupt received. Program terminated. \n");
	exit(1);
}
