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
			case WSA_ASYNC:
			/*control socket async notification message handlers*/
			WSAEvent=WSAGETSELECTEVENT(lParam); /*extract event*/
			WSAErr=WSAGETSELECTERROR(lParam); /*extract error*/
			if(WSAErr)
			{
				/*if error, then display*/
				int i,j;
				for(i=0;j=WSAEVENT;j;i++;j>>=1); /*convert bit into index*/
				WSAperror(WSAErr,aszWSAEvent[i]);
			}
			hSock=(SOCKET)wParam;
			switch(WSAEvent)
			{
			case FD_READ:
				if(!iNextRply)
				{
					/*receive reply from server*/
					iLastRply=RecvFtpRply(hCtrlSock,szFtpRply,RPLY_SIZE);
				}
				if(iLastRply && (iLastRply!=SOCKET_ERROR))
				{
					/*display the reply message*/
					GetDlgItemText(hWinMain,IDC_REPLY,achRplyBuf,RPLY_SIZE-strlen(szFtpRply));
					wsprintf(achTempBuf,"%s%s",stFtpRply,achRplyBuf);
					SetDlgItemText(hWinMain,IDC_REPLY,achTempBuf);
					/*save index to next reply*/
					nRet=strlen(szFtpRply);
					if(iLastRply > nRet+2)
					{
						iNextRply=nRet+3;
						if(szFtpRply[nRet+2])
						iNextRply=nRet+2;
					}
				}
				/*reply based on last command*/
				ProcessFtpRply(szFtpRply,RPLY_SIZE);
				break;
			case FD_WRITE:
				/*send command to server*/
				if(aszFtpCmd[1].nFtpCmd)
					SendFtpCmd();
				break;
			case FD_CONNECT:
				/*control connected at TCP level*/
				nAppState=CTRLCONNECTED;
				wsprintf(achTempBuf,"Server: %s", szHost);
				SetDlgItemText(hDlg,IDC_SERVER,achTempBuf);
				SetDlgItemText(hDlg,IDC_STATUS,"Status: connected");
				break;
			case FD_CLOSE:
				if(nAppState & CTRLCONNECTED)
				{
					nAppState=NOT_CONNECTED; /*reset app status*/
					AbortFtpCmd();
					if(hCtrlSock!=INVALID_SOCKET) /*close control socket*/
						CloseConn(&hCtrlSock, (PSTR)0, 0, hDlg);
					SetDlgItemText(hDlg,IDC_SERVER,"server: none");
					SetDlgItemText(hDlg,IDC_STATUS,"Status: not connected");
				}
				break;
			default:
				break;
			} /*end of switch(WSAEvent) */
			break;
			case WM_COMMAND:
			switch(wParam)
			{
			case IDC_CONNECT:
				/*if we already have a socket, then tell user to close it*/
				if(nAppState & CTRLCONNECTED)
				{
					MessageBox(hDlg,"close the active connection first","unable to connect",MB_OK | MB_ICONASTERISK);
				}
				else
				{
					/*prompt user for server and login user information*/
					bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_SERVER),hDlg,Dlg_Login);
					if(bOk)
					{
						/*check destination address and resolve if necessary*/
						stCRmtName.sin_addr.s_addr=GetAddr(szHost);
						if(stCRmtName.sin_addr.s_addr==INADDR_ANY)
						{
							/*tell user to enter an host*/
							wsprintf(achTempBuf,"Sorry. server %s is invalid. Try again!",szHost);
							MessageBox(hDlg,achTempBuf,"unable to connect!", MB_OK | MB_ICONASTERISK);
						}
						else
						{
							/*initialize connect attempt to server*/
							hCtrlSock=InitCtrlConn(&stCRmtName,hDlg,WSA_ASYNC);
						}
					}
				}
				break;
			case IDC_CLOSE:
				if(nAppState & CTRLCONNECTED)
				{
					/*set application state, so nothing else is processed.*/
					nAppState=NOT_CONNECTED;
					if(hLstn!=INVALID_SOCKET)
					{
						closesocket(hLstnSock);
						hLstnSock=INVALID_SOCKET;
					}
					/*if there is data connection, then abort it*/
					if(hDataSock!=INVALID_SOCKET)
						QueueFtpCmd(ABOR,0);
					/*quit the control connection*/
					if(hCtrlSock!=INVALID_SOCKET)
						QueueFtpCmd(QUIT,0);
					SetDlgItemText(hDlg,IDC_SERVER,"Server: None");
					SetDlgItemText(hDlg,IDC_STATUS,"Status: Not connected");
				}
				break;
			case IDC_RETR:
				/*prompt for name of remote file to get*/
				if(nAppState & CTRLCONNECTED)
				{
					bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hDlg,Dlg_File);
					if(bOk && szDataFile[0])
					{
						if(!bToNul)
						{
							hDatafile=CreateLclFile(szDataFile);
						}
						if(hDataFile!=HFILE_ERROR || bToNul)
						{
							/*tell the server where to connect back*/
							hLstnSock=InitDataConn(&stDLclName,hDlg,WSA_ASYNC + 1);
							if(hLstnSock!=INVALID_SOCKET)
							{
								/*Queue port. type and Retr cmd */
								if(QueueFtpCmd(PORT,0))
								{
									if(QueueFtpCmd(TYPE,"I"))
										QueueFtpCmd(RETR,szDataFile);
								}
							}
						}
					}
				}
				else
					not_connected();
				break;
			case IDC_STOR:
				/*prompt for name of the local file to send*/
				if(nAppState & CTRLCONNECTED)
				{
					bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hDlg,Dlg_File);
					if(bOk && szDataFile[0])
					{
						if(!bFromNul)
						{
							/*if user provided file name, try to open it*/
							hDataFile= _lopen(szDataFile, 0);
							if(hDataFile==HFILE_ERROR)
							{
								wsprintf(achTempBuf,"Unable to open file: %s",szDataFile);
								MessageBox(hWinMain,(LPSTR)achTempBuf,"File error", MB_OK | MB_ICONASTERISK);
							}
						}
						if(hDataFile!=HFILE_ERROR || bFromNul)
						{
							/*tell server where to connect back*/
							hLstnSock=InitDataConn(&stDLclName,hDlg,WSA_ASYNC + 1);
							if(hLstnSock!=INVALID_SOCKET)
							{
								/*queue port,type and STOR cmds*/
								if(QueueFtpCmd(PORT,0))
								{
									if(QueueFtpCmd(TYPE,"I"))
										QueueFtpCmd(STOR,szDataFile);
								}
							}
						}
					}
				}
				else
					not_connected();
				break;
			case IDC_ABOR:
				if(hCtrlSock!=INVALID_SOCKET)
					/*abort the pending FTP command*/
					QueueFtpCmd(ABOR,0);
				break;
			case IDC_LCWD:
				/*prompt for directory and move to it on local system*/
				bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hDlg,Dlg_File);
				if(bOk && aszDataFile[0])
				{
					if(!(_chdir(szDataFile)))
					{
						getcwd(szDataFile,MAXFILENAME-1);
						SetDlgItemText(hDlg,IDC_LPWD,szDataFile);
					}
				}
				break;
			case IDC_LDEL:
				/*prompt for file name and delete it from local system*/
				bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hDlg,Dlg_File);
				if(bOk && szDataFile[0])
				{
					/*if user provided file name, then delete it*/
					remove(szDataFile);
				}
				break;
			case IDC_LDIR:
				/*get local file directory and display in notepad*/
				if(GetLclDir(szTempFile))
				{
					wsprintf(achTempBuf,"notepad %s",szTempFile);
					WinExec(achTempBuf,SW_SHOW);
				}
				break;
			case IDC_RCWD:
				/*prompt for directory and move to it on remote system*/
				if(nAppState & CTRLCONNECTED)
				{
					szDataFile[0]=0;
					bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hDlg,Dlg_File);
					if(bOk && szDataFile[0])
					{
						QueueFtpCmd(CWD,szDataFile);
					}
				}
				else
					not_connected();
				break;
case IDC_RDEL:
				/*prompt for file name and delete it from remote system*/
				if(nAppState & CTRLCONNECTED)
				{
					szDataFile[0]=0;
					bOk=DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hDlg,Dlg_File);
					if(bOk && szDataFile[0])
					{
						/*if user provided file name, send command to delete*/
						QueueFtpCmd(DELE,szDataFile);
					}
				}
				else
					not_connected();
				break;
			case IDC_RDIR:
				/*get remote file directory and display in notepad*/
				if(nAppState & CTRLCONNECTED)
				{
					hDataFile=CreateLclFile(szTempFile);
					if(hDataFile!=HFILE_ERROR)
					{
						/*prepare to receive connection from server*/
						hLstnSock=InitDataConn(&stDLclName,hDlg,WSA_ASYNC + 1);
						if(QueueFtpCmd(PORT,0))
							if(QueueFtpCmd(TYPE,"A"))
								QueueFtpCmd(LIST,0);
					}
				}
				}
				else
					not_connected;
					break;
					case IDC_OPTIONS:
						DialogBox(hInst,MAKEINTRESOURCE(IDD_OPTIONS),hDlg,Dlg_Options);
						break;
					case IDABOUT:
						DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUT),hDlg,hDlg_About);
						break;
					case WM_DESTROY:
					case IDC_EXIT:
						SendMessage(hDlg,WM_COMMAND. IDC_CLOSE, 0L);
						if(hLogFile!=HFILE_ERROR)
							_lclose(hLogFile);
						EndDialog(hDlg,msg);
						bRet=TRUE;
						break;
					default:
						break;
						} /*end of case WM_COMMAND */
						break;
						case WM_INITDIALOG:
							hWinMain=hDlg; /*save main window handle*/
							/*assign an icon to dialog box*/
#ifdef WIN32
							SetClassLong(hDlg,GCL_HICON,(LONG) LoadIcon((HINSTANCE) GetWindowLong(hDlg, GWL_HINSTANCE), __TEXT("AC_FTP")));
#else
							SetClassWord(hDlg,GCW_HICON,(WORD) LoadIcon(hInst,MAKEINTRESOURCE(AC_FTP)));
#endif
							/*initialize FTP command structure*/
							memset(astFtpCmd,0,(sizeof(struct stFtpCmd)) *MAX_CMDS);
							/*display current working directory*/
							getcwd(szDataFile,MAXFILENAME-1);
							SetDlgItemSet(hDlg,IDC_LPWD,szDataFile);
							/*open log file, if logging enabled*/
							if(bLogFile)
							{
								hLogFile= _lcreat (szLogFile,0);
								if(hLogFile==HFILE_ERROR)
								{
									MessageBox(hWinMain,"unable to open log file","file error",MB_OK | MB_ICONASTERISK);
									bLogFile=FALSE;
								}
							}
							/*center dialog box*/
							CenterWnd(hDlg,NULL,TRUE);
							break;
						default:
							break;
							} /*end of switch(msg)*/
							return(bRet);
							} /*end of DlgMain()*/
/*Function: Dlg_Options()
Allow user to change a number of run-time parameters that affect the operation, to allow experimentation & debugging.
*/
BOOL CALLBACK Dlg_Options(HWND hDlg,UINT msg,UINT wParam,LPARAM lParam)
{
	BOOL bRet=FALSE;
	lParam=lParam; /*avoid warning*/
	switch(msg)
	{
	case WM_INITDIALOG:
		CheckDlgButton(hDlg,IDC_TO_NUL,bToNul);
		CheckDlgButton(hDlg,IDC_FROM_NUL,bFromNul);
		CheckDlgButton(hDlg,IDC_LOGFILE,bLogFile);
		CheckDlgButton(hDlg,IDC_IOBEEP,bIOBeep);
		CheckDlgButton(hDlg,IDC_REASYNC,bReAsync);
		CenterWnd(hDlg,hWinMain,TRUE);
		break;
	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_TO_NUL:
			bToNul=!bToNul;
			break;
		case IDC_FROM_NUL:
			bFromNul=!bFromNul;
			break;
		case IDC_LOGFILE:
			bLogFile=!bLogFile;
			break;
		case IDC_IOBEEP:
			bIOBeep=!bIOBeep;
			break;
		case IDC_REASYNC:
			bReAsync=!bReAsync;
			break;
		case IDOK:
			if(bLogFile && hLogFile == HFILE_ERROR)
			{
				bLogFile=FALSE;
		}
			else if(!bLogFile && hLogFile == HFILE_ERROR)
			{
				hLogFile= _lcreat(szLogFile,0);
				if(hLogFile==HFILE_ERROR)
				{
					MessageBox(hWinMain,"unable to open log file","file error",MB_OK | MB_ICONASTERISK);
					bLogFile=FALSE;
				}
			}
			EndDialog(hDlg,TRUE);
			bRet=TRUE;
			break;
		}
	}
	return(bRet);
} /*end of Dlg_Options*/
/*Function:- not_connected()
tell the user that the client needs a connection
*/
void not_connected(void)
{
	MessageBox(hDlgMain,"Requires connection to FTP server","not connected",MB_OK | MB_ICONASTERISK);
}
/* Function: InitCtrlConn()
Get a TCP socket, register for async notification and then, connect to FTP  server*/
SOCKET InitCtrlConn(PSOCKADDR_IN pstName, HWND hDlg, u_int nAsyncMsg)
{
	int nRet;
	SOCKET hCtrlSock;
	/*get TCP socket for control connection*/
	hCtrlSock=socket(AF_INET,SOCK_STREAM,0);
	if(hCtrlSock==INVALID_SOCKET)
	{
		WSAperror(WSAGetLastError(),"socket()");
	}
	else
	{
		/*request async notifications for most events*/
		nRet=WSAAsyncSelect(hCtrlSock,hDlg,nAsyncMsg,(FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE));
		if(nRet==SOCKET_ERROR)
		{
			WSAperror(WSAGetLastError(),"WSAAsyncSelect()");
			closesocket(hCtrlSock);
			hCtrlSock=INVALID_SOCKET;
		}
		else
		{
			/*initiate non-blocking connect to server*/
			pstName->sin_family=PF_INET;
			pstName->sin_port=htons(IPPORT_FTP);
			nRet=connect(hCtrlSock,(LPSOCKADDR)pstName,SOCKADDR_LEN);
			if(nRet==SOCKET_ERROR)
			{
				int WSAErr=WSAGetLastError();
				if(WSAErr!=WSAEWOULDBLOCK)
				{
					/*report error and clean up*/
					WSAperror(WSAErr,"connect()");
					closesocket(hCtrlSock);
					hCtrlSock=INVALID_SOCKET;
				}
			}
		}
	}
	return(hCtrlSock);
} /*end of InitCtrlConn()*/
/*--Function:- SendFtpCmd()
Format and send a FTP command to the server */
int SendFtpCmd(void)
{
	int nRet,nLen,nBytesSent=0;
	int nFtpCmd=astFtpCmd[1].nFtpCmd;
	/*create a command string*/
	if(szFtpCmd[0]==0)
	{
		switch(nFtpCmd)
		{
		case PORT:
			wsprintf(szFtpCmd,"PORT %d,%d,%d,%d,%d,%d \r\n",
				stDlclName.sin_addr.S_un.S_un_b.s_b1,/*local addr*/
				stDlclName.sin_addr.S_un.S_un_b.s_b2,
				stDlclName.sin_addr.S_un.S_un_b.s_b3,
				stDlclName.sin_addr.S_un.S_un_b.s_b4,
				stDlclName.sin_port & 0XFF, /*local port*/
				(stDlclName.sin_port & 0XFF00)>>8);
			break;
		case CWD:
		case DELE:
		case PASS:
		case RETR:
		case STOR:
		case TYPE:
		case USER:
			/*FTP command and parameter*/
			wsprintf(szFtpCmd,"%s%s\r\n",aszFtpCmd[nFtpCmd],&(astFtpCmd[1].szFtpParm));
			break;
		case ABOR:
		case LIST:
		case PWD:
		case QUIT:
			/*solitary FTP command string (no parameters)*/
			wsprintf(szFtpCmd,"%s\r\n",aszFtpCmd[nFtpCmd]);
			break;
		default:
			return(0);
		}
	}
	nLen=strlen(szFtpCmd);
	if(hCtrlSock!=INVALID_SOCKET)
	{
		/*send FTP command to control socket*/
		while(nBytesSent < nLen)
		{
			nRet=send(hCtrlSock,(LPSTR)szFtpCmd,nLen-nBytesSent,0);
			if(nRet==SOCKET_ERROR)
			{
				int WSAErr=WSAGetLastError();
				if(WSAErr!=WSAEWOULDBLOCK)
					WSAperror(WSAErr,"SendFtpCmd()");
				break;
			}
			nBytesSent+=nRet;
		}
	}
	if(nBytes==nLen)
	{
		int i;
		if(nFtpCmd==PASS) /*hide password*/
			memset(szFtpCmd+5,'x',10);
		if(bLogFile) /*log command*/
			_lwrite(hLogFile,szFtpCmd,strlen(szFtpCmd));
		GetDlgItemText(hWinMain,IDC_REPLY,/*display command*/ achRplyBuf,"%s%s",szFtpCmd,achRplyBuf);
		SetDlgItemText(hWinMain,IDC_REPLY,achTempBuf);
		szFtpCmd[0]=0; /*disable FTP command string*/
		/*move everything up in the command queue*/
		for(i=0;i<nQLen;i++)
		{
			astFtpCmd[i].nFtpCmd=astFtpCmd[i+1].nFtpCmd;
			astFtp[i+1].nFtpCmd=0; /*reset old command*/
			if(*(astFtpCmd[i+1].szFtpParm))
			{
				memcpy(astFtpCmd[i].szFtpParm,astFtpCmd[i+1].szFtpParm,CMD_SIZE);
				*(astFtpCmd[i+1].szFtpParm)=0; /*terminate old string*/
			}
			else
			{
				*(astFtpCmd[i].szFtpParm)=0; /*terminate unused string*/
			}
		}
		nQLen--; /*decrement the queue length*/
		switch(nFtpCmd)
		{
		case(USER):
			SetDlgItemText(hWinMain,IDC_STATUS,"Status:connecting");
			break;
		case(STOR):
			SetDlgItemText(hWinMain,IDC_STATUS,"Status:sending a file");
			break;
		case(RETR):
			SetDlgItemText(hWinMain,IDC_STATUS,"Status:receiving a file");
			break;
		case(LIST):
			SetDlgItemText(hWinMain,IDC_STATUS,"Status:Receiving directory");
			break;
		case(QUIT):
			SetDlgItemText(hWinMain,IDC_STATUS,"Server:None");
			SetDlgItemText(hWinMain,IDC_STATUS,"Status:Not connected");
			break;
		}
	}
	return(nBytesSent);
	} /*end of SendFtpCmd()*/
	/*---Function: QueueFtpCmd()
	Put FTP command in command queue for sending after we receive responses to pending commands*/
	BOOL QueueFtpCmd(int nFtpCmd,LPSTR szFtpParm)
	{
		if((nFtpCmd==ABOR) || (nFtpCmd==QUIT))
		{
			AbortFtpCmd();
			if(hCtrlSock!=INVALID_SOCKET)
				SetDlgItemText(hWinMain,IDC_STATUS:"Status:Connected");
		}
		else if(nQLen==MAX_CMDS)
		{
			/*notify users if they fit in the queue*/
			MessageBox(hWinMain, "FTP queue is full, try again later", "unable to queue command", MB_OK | MB_ICONASTERISK);
			return(FALSE); /*not queued*/
		}
		nQLen++; /*increment FTP command counter*/
		/*save command*/
		astFtpCmd[nQLen].nFtpCmd=nFtpCmd;
		if(szFtpParm)
			lstrcpy(astFtpCmd[nQLen].szFtpParm,szFtpParm);
		if(!(astFtpCmd[0].nFtpCmd) && astFtpCmd[1].nFtpCmd)
		{
			SendFtpCmd();
		}
		return(TRUE); /*queued!*/
	} /*end of QueueFtpCmd() */
	/*--Function: AbortFtpCmd()
	Clean up routine to abort a pending FTP command and clear the command queue*/
	void AbortFtpCmd(void)
	{
		int i;
		if(hLstnSock!=INVALID_SOCKET)
			/*closing the listen socket*/
			closesocket(hLstnSock);
		hLstnSock=INVALID_SOCKET;
	}
	if(hDataSock!=INVALID_SOCKET)
		/*closing data socket*/
		CloseConn(&hDataSock,(astFtpCmd[0].nFtpCmd!=STOR) ? achInBuf : (PSTR)0, INPUT_SIZE, hWinMain);
	EndData();
	}
	for(i=0;i<MAX_CMDS;i++)
	{
		/*clear command queue*/
		astFtpCmd[i].nFtpCmd=0;
		nQLen=0;
	} /*end of AbortFtpCmd()*/
/*--Function: RecvFtpRply()
	Read the FTP reply from the server */
	int RecvFtpRply(SOCKET hCtrlSock, LPSTR szFtpRply, int nLen)
	{
		int nRet=0;
		if(hCtrlSock!=INVALID_SOCKET)
		{
			memset(szFtpRply,0,nLen); /*init receive buffer*/
			/*reading*/
			nRet=recv(hCtrlSock,(LPSTR)szFtpRply,nLen,0);
			if(nRet==SOCKET_ERROR)
			{
				int WSAErr=WSAGetLastError();
				if(WSAErr!=WSAEWOULDBLOCK)
				{
					WSAperror(WSAErr,"RecvFtpRply()");
				}
			}
			else if(bLogFile) /*log reply*/
				_lwrite(hLogFile,szFtpRply,nRet);
		}
		return(nRet);
	} /*end of RecvFtpRply()*/
