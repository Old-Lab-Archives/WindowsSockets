#include "winsock.h"
#include "winsockx.h"
#include "async-ftp-client.h"
#include "WSA_Xtra.h"
#include<windows.h>
#include<windowsx.h>
#include<stdio.h>
#include<dos.h>
#include<direct.h>
#include "resource.h"
#ifdef _WIN32
#include<io.h> /* Microsoft 32-bit FIND file structure */
#else
#include<direct.h> /* Microsoft 16-bit FIND file structure */
#endif
/* FTP command strings */
LPSTR aszFtpCmd[]={"","CWD","DELE","PASS","PORT","RETR","STOR","TYPE","USER","ABOR","LIST","PWD","QUIT"};
char szAppName[]="AC_FTP";
BOOL nAppState=NOT_CONNECTED; /*application state*/
BOOL bToNul=FALSE; /*get to NUL device file*/
BOOL bFromNul=FALSE; /*put from Nul device file*/
BOOL bIOBeep=FALSE; /*beep on FD_READ, FD_WRITE*/
BOOL bDebug=FALSE; /*debug output to WinDebug*/
BOOL bReAsync=TRUE; /*call WSAAsyncSelect after accept()*/
BOOL bLogFile=TRUE; /*write commands and replies to log file*/
SOCKET hCtrlSock=INVALID_SOCKET; /*Ftp control socket*/
SOCKET hLstnSock=INVALID_SOCKET; /*listening data socket*/
SOCKET hDataSock=INVALID_SOCKET; /*connected data socket*/
char szHost[MAXHOSTNAME]={0}; /*remote host name or address*/
char szUser[MAXUSERNAME]={0}; /*user id*/
char szPwrd[MAXPASSWORD]={0}; /*user password*/
SOCKADDR_IN stCLclName; /*control socket name(local client)*/
SOCKADDR_IN stCRmtName; /*control socket name(remote client)*/
SOCKADDR_IN stDLclName; /*data socket name(local client)*/
SOCKADDR_IN stDRmtName; /*data socket name(remote client)*/
char achInBuf[INPUT_SIZE]; /*network input data buffer*/
char achOutBuf[INPUT_SIZE]; /*network output buffer*/
char szFtpRply[RPLY_SIZE]={0}; /*FTP reply buffer*/
char szDataFile[MAXFILENAME]={0}; /*file name*/
char szFtpCmd[CMD_SIZE]={0}; /* FTP command buffer */
char achRplyBuf[BUF_SIZE]; /*reply display buffer*/
/* index=0 is waiting for a reply, whereas index=1 is next one to be sent (pending) */
FTPCMD astFtpCmd[MAX_CMDS]; /*FTP command queue*/
int nQLen; /*number of entries in FTP command queue*/
int nFtpRplyCode; /*FTP reply code from server*/
int iNextRply; /*index to next reply string*/
int iLastRply;
HFILE hDataFile=HFILE_ERROR; /*file handle for open data file*/
LONG lStartTime; /*start time for data transfer*/
LONG lByteCount;
char szLogFile[]="ac_ftp.log"; /*FTP command and reply log file*/
HFILE hLogFile=HFILE_ERROR;
/*---Function: WinMain()
Initialize and start message loop
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	MSG msg;
	int nRet;
	lpszCmdLine=lpszCmdLine; /*avoid warning*/
	hPrevInstance=hPrevInstance;
	nCmdShow=nCmdShow;
	hInst=hInstance; /*save instance handle*/
	/*--Initialize WinSock DLL--*/
	nRet=WSAStartup(WSA_VERSION,&stWSAData);
	/* WSAStartup() returns error value if failed */
	if(nRet!=0)
	{
		WSAperror(nRet,"WSAStartup()");
		/*Don't ever dare to continue if we can't use a WinSock*/
	}
	else
	{
		DialogBox(hInst, MAKEINTRESOURCE(AC_FTP),NULL,Dlg_Main);
		/*--release WinSock DLL --*/
		nRet=WSACleanup();
		if(nRet==SOCKET_ERROR)
			WSAperror(WSAGetLastError(),"WSACleanup()");
	}
	return msg.wParam;
} /*end of WinMain() */
/*---Function: Dlg_Main()
Doing all the message procssing for the main dialog box
*/
BOOL CALLBACK Dlg_Main(HWND hDlg,UINT msg,UINT wParam,LPARAM lParam)
{
	int nAddrSize=sizeof(SOCKADDR);
	WORD WSAEvent, WSAErr;
	SOCKET hSock;
	BOOL bOK, bRet=FALSE;
	int nRet;
	LONG lRet;
	switch(msg)
	{
		case WSA_ASYNC+1;
			/*Data socket async notification message handlers*/
			hSock=(SOCKET)wParam; /*socket*/
			WSAEvent=WSAGETSELECTEVENT(lParam); /*extract event*/
			WSAErr=WSAGETSELECTERROR(lParam); /*extract error*/
			/*if there is error, then display it to the user, hoping that 'listen socket' shouldn't be one of those culprits, but some Winsocks have a tiny possibility
			*/
			if((WSAErr) && (hSock==hDataSock))
			{
				int i,j;
				for(i=0;j=WSAEvent;j;i++;j>>=1); /*convert bit into index*/
				WSAperror(WSAErr,aszWSAEvent[i]);
			}
			switch(WSAEvent)
			{
			case FD_READ:
				if(bIOBeep)
					MessageBeep(0XFFFF);
				if(hDataSock!=INVALID_SOCKET)
				{
					/*receive file data or directory list*/
					RecvData(hDataSock,hDataFile,achInBuf,INPUT_SIZE);
				}
				break;
			case FD_ACCEPT:
				if(hLstnSock!=INVALID_SOCKET)
				{
					/*accept the incoming data connection request*/
					hDataSock=AcceptDataConn(hLstnSock,&stDRmtName);
					nAppState |= DATACONNECTED;
					/*close the listening socket*/
					closesocket(hLstnSock);
					hLstnSock=INVALID_SOCKET;
					lStartTime=GetTickCount();
					/*data transfer should begin with FD_WRITE or FD_READ */
				}
			case FD_WRITE:
				/*send file data*/
				if(astFtpCmd[0].nFtpCmd==STOR)
				{
					lRet=SendData(&hDataSock,hDataFile,MTU_SIZE);
				}
				break;
			case FD_CLOSE: /*data connection closed*/
				if(hSock==hDataSock)
				{
					/*read remaining data into buffer and close connection*/
					CloseConn(&hDataSock,(astFtpCmd[0].nFtpCmd!=STOR) ? achInBuf : (LPSTR)0, INPUT_SIZE,hDlg);
					EndData();
				}
					break;
			default:
				break;
			} /*end of switch(WSAEvent) */
				break;
