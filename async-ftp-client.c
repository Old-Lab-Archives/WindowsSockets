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
/*--Function: ProcessFtpRply()
	Figuring out what happened and what to do next
	*/
	void ProcessFtpRply(LPSTR szRply, int nBufLen)
	{
		LPSTR szFtpRply;
		int nPendingFtpCmd, i;
		szFtpRply=szRply;
		while((*(szFtpRply+3)=='-')||((*(szFtpRply)==' ')&&(*(szFtpRply+1)==' ')&&(*(szFtpRply+2)==' ')))
		{
			/*find end of reply line*/
			for(i=0;*szFtpRply!=0X0a && *szFtpRply && i<nBufLen-3;szFtpRply++,i++);
			szFtpRply++; /*goto beginning of next reply*/
			if(!(*szFtpRply)) /*quit if end of string*/
				return;
		}
		*szFtpCmd=0; /*disable old command string*/
		nPendingFtpCmd=astFtpCmd[0].nFtpCmd; /*save last FTP command*/
		if((*szFtpRply!='1')&&(nPendingFtpCmd!=LIST)&&(nPendingFtpCmd!=STOR)&&(nPendingFtpCmd!=RETR))
			/*for any starter reply, clear old commands*/
			astFtpCmd[0].nFtpCmd=0;
		/*first digit in 3-digit FTP reply code is the most significant*/
		switch(*szFtpRply)
		{
		case('1'): /*positive preliminary reply*/
			break;
		case('2'): /*positive completion reply*/
			switch(nPendingFtpCmd)
			{
			case 0:
				/*check if service ready for user*/
				if((*(szFtpRply+1)=='2') && (*(szFtpRply+2)=='0'))
					QueueFtpCmd(USER, szUser);
				break;
			case CWD:
			case USER:
			case PASS:
				/*Duh-uh! We are logged in now fellas. Get remote working directory */
				QueueFtpCmd(PWD,0);
				break;
			case PWD:
				/*display remote working directory*/
				SetDlgItemText(hWinMain,IDC_RPWD,&szFtpRply[4]);
				break;
			case TYPE:
			case PORT:
				/*send next command if it's already queued*/
				SendFtpCmd();
				break;
			case ABOR:
				/*close the data socket*/
				if(hDataSock!=INVALID_SOCKET)
					CloseConn(&hDataSock,(PSTR)0,0,hWinMain);
				break;
			case QUIT:
				/*close the control socket*/
				if(hCtrlSock!=INVALID_SOCKET)
					CloseConn(&hCtrlSock,(PSTR)0,0,hWinMain);
				break;
			default:
				break;
			}
			break;
		case ('3'): /*positive intermediate reply*/
			if(nPendingFtpCmd==USER)
				QueueFtpCmd(PASS, szPwrd);
			break;
		case ('4'): /*transient negative completion reply*/
		case ('5'): /*permanent negative completion reply*/
			/*if port failed, forget about queued commands*/
			if(nPendingFtpCmd!=ABOR)
				QueueFtpCmd(ABOR,0);
			break;
		}
	} /*end of ProcessFtpRply()*/
/*--Function: InitDataConn()
	Setup a listening socket for data connection */
	SOCKET InitDataConn(PSOCKADDR_IN lpstName,HWND hDlg,u_int nAsyncMsg)
	{
		int nRet;
		SOCKET hLstnSock;
		int nLen=SOCKADDR_LEN;
		/*get a TCP socket to use for data connection listen*/
		hLstnSock=socket(AF_INET,SOCK_STREAM,0);
		if(hLstnSock==INVALID_SOCKET)
		{
			WSAperror(WSAGetLastError(),"socket()");
		}
		else
		{
			/*request async notification for most events*/
			nRet=WSAAsyncSelect(hLstnSock,hDlg,nAsyncMsg,(FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE));
			if(nRet==SOCKET_ERROR)
			{
				WSAperror(WSAGetLastError(),"WSAAsyncSelect()");
			}
			else
			{
				/*name the local socket with bind()*/
				lpstName->sin_family=PF_INET;
				lpstName->sin_port=0;
				nRet=bind(hLstnSock,(LPSOCKADDR)lpstName,SOCKADDR_LEN);
				if(nRet==SOCKET_ERROR)
				{
					WSAperror(WSAGetLastError(),"bind()");
				}
				else
				{
					/*get local port number assigned by bind()*/
					nRet=getsockname(hLstnSock,(LPSOCKADDR)lpstName,(int FAR *)&nLen);
					if(nRet==SOCKET_ERROR)
					{
						WSAperror(WSAGetLastError(),"getsockname()");
					}
					else
					{
						/*listen for incoming connection requests*/
						nRet=listen(hLstnSock,5);
						if(nRet==SOCKET_ERROR)
						{
							WSAperror(WSAGetLastError(),"listen()");
						}
					}
				}
			}
			/*if we haven't had error, also don't know local IP address, then we gotta try before we return*/
			if(!lpstName->sin_addr.s_addr)
			{
				lpstName->sin_addr.s_addr=GetHostID( );
				if(!lpstName->sin_addr.s_addr)
				{
					MessageBox(hDlg,"unable to get local IP address","InitConn( ) Failed", MB_OK | MB_ICONASTERISK);
					nRet=SOCKET_ERROR;
				}
			}
			/*if we get error and yet don't know our IP address, we are in deep shit. Clean up! */
			if(nRet==SOCKET_ERROR)
			{
				closesocket(hLstnSock);
				hLstnSock=INVALID_SOCKET;
			}
		}
		return(hLstnSock);
	} /*end of InitDataConn()*/
/*Sample libraries added:
					1. CenterWnd() - Moves window to center of parent window
					2. CloseConn() - standard TCP close/shutdown
					3. CreateLclFile() - create a file on local system
					4. Dlg_About() - displays contents
					5. GetAddr() - returns address value for host name or address string
					6. GetBuf() - Gets the largest possible send or receive buffer upto a limit
					7. GetHostID() - Gets local IP address
					8. GetLclDir() - Gets the file directory
					9. GetPort() - Gets Port number
					10. WSAErrStr() - Copies short error description for WinSock into a buffer
					11. WSAperror() - Displays Winsock error value
					*/
/*Function: CenterWnd()
Center window relative to the parent window */
void CenterWnd(HWND hWnd, HWND hParentWnd, BOOL bPaint)
{
RECT rc2, rc1;
RECT FAR, *lprc;
int nWidth, nHeight, cxCenter, cyCenter;
if(!hParentWnd) /*if we've no parent, then use desktop*/
hParentWnd=GetDesktopWindow();
GetWindowRect(hParentWnd,&rc2);
lprc=(RECT FAR *)&rc2;
cXCenter=lprc->left+((lprc->right-lprc->left)/2);
cyCenter=lprc->top+((lprc->bottom-lprc->top)/2);
MoveWindow(hWnd, cXCenter-(nWidtth/2), cyCenter-(nHeight/2), nWidth, nHeight, bPaint);
return;
} /*end of CenterWnd() */
/*Function: CloseConn()
Closing TCP connection, ensuring that no data loss can occur on WInSocks that post FD_CLOSE when data is still available to read
*/
int CloseConn(SOCKET hSock, LPSTR achInBuf, int len, HWND hWnd)
{
int nRet;
char achDiscard(BUF_SIZE);
int cbBytesToDo=len, cbBytesDone=0;
if(hSock!=INVALID_SOCKET)
{
/*disable asynchronous notification if window handle is provided*/
if(hWnd)
{
nRet=WSAAsyncSelect(hSock, hWnd, 0, 0);
if(nRet==SOCKET_ERROR)
WSAperror(WSAGetLastError(),"CloseConn() WSAAsyncSelect()");
}
/*half-closing the connecting*/
nRet=shutdown(hSock,l);
/*read remaining data*/
for(nRet=1;(nRet && (nRet!=SOCKET_ERROR));)
{
if(achInBuf)
{
nRet=recv(hSock,&achInBuf[cbBytesDone],cbBytesToDo,0);
if(nRet && (nRet!=SOCKET_ERROR))
{
cbBytesToDo-=nRet;
cbBytesDone+=nRet;
}
}
else
{
/*no buffer provided, so discard any data*/
nRet=recv(hSock,achDiscard,BUF_SIZE,0);
}
}
/*closing the socket*/
closesocket(hSock);
}
return(nRet);
} /*end of CloseConn()*/
/*Function: CreateLclFile()
Try to create a file on local system and notify user if fails and then prompt for new file name*/
HFILE CreateLclFile(LPSTR szFileName)
{
HFILE hFile;
char szRmtFile(MAXFILENAME);
hFile= _lcreat (szFileName, 0); /*creating the file*/
strcpy(szRmtFile,szFileName); /*save remote file name*/
while(hFile==HFILE_ERROR)
{
wsprintf(achTempBuf,"Unable to create the file %s, Change the file name", szFileName);
MessageBox(hWinMain,achTempBuf,"File Error",MB_OK | MB_ICONASTERISK);
if(!DialogBox(hInst,MAKEINTRESOURCE(IDD_FILENAME),hWinMain,Dlg_File))
{
/*no new file name is provided, so quit*/
break;
}
else
{
/*try creating a new file*/
hFile= _lcreat(szFileName, 0);
}
}
strcpy(szFileName,szRmtFile); /*replace remote filename*/
return hFile;
} /*end of CreateLclFile() */
/*Function: Dlg_About()
Displays and WinSock DLL info */
BOOL CALLBACK Dlg_About
{
HWND hDlg;
UINT msg;
UINT wParam;
LPARAM lParam;
{
char achDataBuf[WSADESCRIPTION_LEN+1];
lParam=lParam; /*avoid warning*/
switch(msg)
{
case WM_INITDIALOG:
wsprintf(achDataBuf,"(Compiled: %s, %s) \n",(LPSTR)__DATE__,(LPSTR)__Time__);
SetDlgItemText(hDlg, IDC_COMPILEDATE,(LPCSTR)achDataBuf);
wsprintf(achDataBuf, "Version:%d.%d",LOBYTE(stWSAData.wVersion)/*major version*/,HIBYTE(stWSAData.wVersion))/*minor version*/;
SetDlgItemText(hDlg,IDS_DLLVER,(LPCSTR)achDataBuf);
wsprintf(achDataBuf,"NewVersion:%d.%d",LOBYTE(stWSAData.wVersion)/*major version*/,HIBYTE(stWSAData.wVersion))/*minor version*/;
SetDlgItemText(hDlg,IDS_DLLHIVER,achDataBuf);
SetDlgItemText(hDlg,IDS_DESCRIP,(LPCSTR)(stWSAData.szDescription));
SetDlgItemText(hDlg,IDS_STATUS,(LPCSTR)(stWSAData.szSystemStatus));
wsprintf(achDataBuf,"MaxSockets: %u",stWSAData.iMaxSockets);
SetDlgItemText(hDlg,IDS_MAXSOCKS,(LPCSTR)achDataBuf);
wsprintf(achDataBuf,"iMaxUdp: %u",stWSAData.iMaxUdpDg);
SetDlgItemText(hDlg,IDS_MAXUDP,(LPCSTR)achDataBuf);
/*center dialog box*/
CenterWnd(hDlg,hDlgMain,TRUE);
return FALSE;
case WM_COMMAND:
switch(wParam)
{
case IDOK:
EndDialog(hDlg,0);
return TRUE;
}
break;
}
return FALSE;
} /*end of Dlg_About()*/
/*Function:-- GetAddr()
Given a string, it will return an IP address. First, it tries to convert directly. if it fails, it tries to resolve as host name
*/
u_long GetAddr(LPSTR,szHost)
{
LPHOSTENT lpstHost;
u_long lAddr=INADDR_ANY;
/*check if string exists*/
if(*szHost)
{
/*check for a dotted IP address string*/
lAddr=inet_addr(szHost);
/*if not an address, then trying to resolve it as host name*/
if((lAddr==INADDR_NONE) && (_fstrcmp (szHost,"255.255.255.255")))
{
lpstHost=gethostbyname(szHost);
if(lpstHost)
{
/*success*/
lAddr= *((u_long FAR *) (lpstHost->h_addr));
}
else
{
lAddr=INADDR_ANY; /*failure*/
}
}
}
return(lAddr);
} /*end of GetAddr()*/
/*Function: GetBuf()
Getting send/receive buffer space as WinSock starts with the amount requested */
int GetBuf(SOCKET hSock, int nBigBufSize, int nOptval);
int nRet, nTrySize, nFinalSize=0;
for(nTrySize=nBigBufSize;nTrySize>MTU_SIZE;nTrySize>>=1)
{
nRet=setsockopt(hSock,SOL_SOCKET,nOptval,(char FAR*)&nTrySize,sizeof(int));
if(nRet==SOCKET_ERROR)
{
int WSAErr=WSAGetLastError();
if((WSAErr==WSAENOPROTOOPT)||(WSAErr==WSAEINVAL))
break;
}
else
{
nRet=sizeof(int);
getsockopt(hSock,SOL_SOCKET,nOptval,(char FAR*)&nFinalSize,&nRet);
break;
}
}
return(nFinalSize);
} /*end of GetBuf() */
/* Function:---- GethostID()
Getting host IP address using this algorithm:
~~ Get local host name with gethostname()
~~ Attempt to resolve local host name with gethostbyname()
If that fails, then
~~ Get UDP socket
~~ Connect UDP socket to arbitrary address and port
~~ Use getsockname() to get local address
*/
LONG GetHostID()
{
char szLclHost [MAXHOSTNAME];
LPHOSTENT lpstHostent;
SOCKADDR_IN stLclAddr;
SOCKADDR_IN stRmtAddr;
int nAddrSize=sizeof(SOCKADDR);
SOCKET hSock;
int nRet;
/*init local address to zero*/
stLclAddr.sin_addr.s_addr=INADDR_ANY;
/*get local host name*/
nRet=gethostname(szLclHost,MAXHOSTNAME);
if(nRet!=SOCKET_ERROR)
{
/*resolve host name for local address*/
lpstHostent=gethostbyname((LPSTR)szLclHost);
if(lpstHostent)
stLclAddr.sin_addr.s_addr= *((u_long FAR*) (lpstHostent->h_addr));
}
/*if not yet resolved, then try second strategy*/
if(stLclAddr.sin_addr.s_addr==INADDR_ANY)
{
/*Get UDP socket*/
hSock=socket(AF_INET,SOCK_DGRAM,0);
if(hSock!=INVALID_SOCKET)
{
/*connect to arbitrary port and address*/
stRmtAddr.sin_family=AF_INET;
stRmtAddr.sin_port=htons(IPPORT_ECHO);
stRmtAddr.sin_addr.s_addr=inet_addr("128.127.50.1");
nRet=connect(hSock,(LPSOCKADDR)&stRmtAddr,sizeof(SOCKADDR));
if(nRet!=SOCKET_ERROR)
{
/*get local address*/
getsockname(hSock,(LPSOCKADDR)&stLclAddr,(int FAR*)&nAddrSize);
}
closesocket(hSock); /*Woo hoo! Good night socket. */
}
}
return(stLclAddr.sin_addr.s_addr);
} /*End of GetHostID() */
/*--Function: GetLclDir()
Temporary file for later display */
BOOL GetLclDir(LPSTR szTempFile)
{
#ifdef WIN32
struct _finddata_t stFile; /* Microsoft's 32-bit 'Find' file structure */
#else
struct _find_t stFile; /* Microsoft's 16-bit 'Find' file structure */
#endif
HFILE hTempFile;
int nNext;
hTempFile = CreateLclFile(szTempFile);
if(hTempFile!=HFILE_ERROR)
{
#ifdef WIN32
nNext =_findfirst("*.*",&stFile);
while(!nNext)
{
wsprintf(achTempBuf,"%-12s %.24s %9ld\n",stFile.name,ctime(&(stFile.time_write)),stFile.size);
_lwrite(hTempFile,achTempBuf,strlen(achTempBuf));
nNext= _findnext(nNext, &stFile);
}
#else
nNext= _dos_findfirst("*.*",0,&stFile);
while(!nNext)
{
unsigned month,day,year,hour,second,minute;
month=(stFile.wr_date>>5) & 0XF;
day=stFile.wr_date & 0X1F;
year=((stFile.wr_date>>9) & 0X7F) + 80;
hour=(stFile.wr_time>>11) & 0X1F;
minute=(stFile.wr_time>>5) & 0X3F;
second=(stFile.wr_time & 0X1F) << 1;
wsprintf(achTempBuf,"%s\t\t%ld bytes \t%d-%d-%d \t%.2d:%.2d:%.2d\r\n",stFile.name,stFile.size,month,day,year,hour,minute,second);
_lwrite(hTempFile,achTempBuf,strlen(achTempBuf));
nNext= _dos_findnext(&stFile);
}
#endif
_lclose(hTempFile);
return(TRUE);
}
return(FALSE);
} /* end of GetLclDir() */
/*Function: GetPort()
Returns a port number from a string. It involves converting from ASCII to integer or resolving as service name.
This function is limited since it assumes the service name that will not begin with an integer where it is possible like we say "Cat on the wall!"
*/
u_short GetPort(LPSTR szService)
{
u_short nPort=0; /*Port 0 is invalid*/
LPSERVENT lpServent;
char c;
c= *szService;
if((c>='1') && (c<='9'))
{
/*convert ASCII to integer and put in network order*/
nPort=htons((u_short)atoi (szService));
}
else
{
/*resolve service name to port number*/
lpServent=getservbyname((LPSTR)szService,(LPSTR)"tcp");
if(!lpServent)
{
WSAperror(WSAGetLastError( ),"getservbyname( )");
}
else
{
nPort=lpServent->s_port;
}
}
return(nPort);
} /*end of GetPort() */
/*Function: GetWSAErrStr()
Given a winsock error value, return error string
*/
int GetWSAErrStr(int WSAErr, LPSTR lpErrBuf)
{
int err_len=0;
HANDLE hInst;
HWND hwnd;
hwnd=GetActiveWindow();
hInst=GetWindowWord(hwnd,GWW_HINSTANCE);
if(WSAErr==0)
WSAErr=WSABASEERR; /*base resource file number*/
if(WSAErr >= WSABASEERR) /*valid error code*/
/*get error string in the table from the resource file*/
err_len=LoadString(hInst,WSAErr,lpErrBuf,ERR_SIZE/2);
return(err_len);
} /*end of GetWSAErrStr() */
/*Function: WSAperror()
Displays the input parameter string and string description that corresponds to winsock error value input parameter */
void WSAperror(int WSAErr,LPSTR szFuncName)
{
static char achErrBuf[ERR_SIZE]; /*buffer for errors*/
static char achErrMsg[ERR_SIZE/2];
WSAErrStr(WSAErr,achErrMsg);
wsprintf(achErrBuf,"%s failed,%-40c\n\n%s",szFuncName,' ',achErrMsg);
/*display error message, it doesn't matter whether it's complete or not */
MessageBox(GetActiveWindow,achErrBuf,"Error",MB_OK | MB_ICONHAND);
return;
} /*end of WSAperror() */
/*Function: AcceptDataConn()
Accept an incoming data connection
*/
SOCKET AcceptDataConn(SOCKET hLstn, PSOCKADDR_IN pstName)
{
	SOCKET hDataSock;
	int nRet, nLen=SOCKADDR_LEN, nOptval;
	hDataSock=accept(hLstnSock,(LPSOCKADDR)pstName,(LPINT)&nLen);
	if(hDataSock==SOCKET_ERROR)
	{
		int WSAErr=WSAGetLastError();
		if(WSAErr!=WSAEWOULDBLOCK)
			WSAperror(WSAErr,"accept");
	}
	else if(bReAsync)
	{
		nRet=WSAAsyncSelect(hDataSock,hWinMain,WSA_ASYNC+1,(FD_READ | FD_WRITE | FD_CLOSE));
		if(nRet==SOCKET_ERROR)
		{
			WSAperror(WSAGetLastError(),"WSAAsyncSelect()");
		}
		/*getting buffer space*/
		nOptval=astFtpCmd[0].nFtpCmd==STOR ? SO_SNDBUF : SO_RCVBUF;
		GetBuf(hDataSock,INPUT_SIZE*2,nOptval);
	}
	return(hDataSock);
} /*end of AcceptDataConn()*/
/*--Function: SendData()
Open data file, read and send */
long SendData(SOCKET *hDataSock, HFILE hDataFile, int len)
{
	static int cbReadFromFile; /* Bytes read from file */
	static int cbSentToServer; /*number of buffered bytes sent*/
	static HFILE hLastFile; /*handle of last file sent*/
	long cbTotalSent=0; /*total bytes sent*/
	int nRet,WSAErr,cbBytesToSend;
	/*reset our counters when we access a new file*/
	if(hLastFile!=hDataFile)
	{
		cbReadFromFile=0;
		cbSentToServer=0;
		hLastFile=hDataFile;
	}
	/*read data from file and send it*/
	do
	{
		if(bIOBeep)
			MessageBeep(0XFFFF);
		/*calculate what's left to send*/
		cbBytesToSend=cbReadFromFile - cbSentToServer;
		if(cbBytesToSend <= 0)
		{
			/*read data from input file*/
			if(!bFromNul)
			{
				cbReadFromFile= _lread(hDataFile,achOutBuf,INPUT_SIZE);
				if(cbReadFromFile==HFILE_ERROR)
				{
					MessageBox(hWinMain,"error reading data file","SendData( ) failed",MB_OK | MB_ICONASTERISK);
					break;
				}
				else if(!cbReadFromFile)
				{
					/*no more data to send*/
					CloseConn(hDataSock,(PSTR)0,0,hWinMain);
					EndData( );
					break;
				}
				else
				{
					cbBytesToSend=cbReadFromFile;
				}
			}
			else
			{
					/*just send whatever is in memory*/
					if(lByteCount < MAXNULPUT)
					{
						cbBytesToSend=INPUT_SIZE;
					}
					else
					{
						CloseConn(hDataSock,(PSTR)0,0,hWinMain);
						EndData();
					}
				}
				cbSentToServer=0; /*reset tally*/
			}
				/*send data to server*/
				nRet=send(*hDataSock, &(achOutBuf[cbSentToServer]),((len<cbBytesToSend) ? len : cbBytesToSend), 0);
				if(nRet==SOCKET_ERROR)
				{
					WSAErr=WSAGetLastError();
					/*display significant errors*/
					if(WSAErr!=WSAEWOULDBLOCK)
						WSAperror(WSAErr,(LPSTR)"send()");
				}
				else
				{
					/*update byte counter and display*/
					lByteCount+=nRet;
					_ltoa(lByteCount,achTempBuf,10);
					SetDlgItemText(hWinMain,IDC_DATA_RATE,achTempBuf);
					cbSentToServer+=nRet; /*tally bytes sent since last file read*/
					cbTotalSent+=nRet; /*tally total bytes sent since started*/
				}
	}
			while(nRet!=SOCKET_ERROR);
			return(cbTotalSent);
}/*end of SendData()*/
/*--- Function: RecvData()
Receive data from net and write to open the data file */
int RecvData(SOCKET hDataSock,HFILE hDataFile,LPSTR achInBuf,int len)
{
	static HFILE hLastFile; /*handle of last file sent*/
	static int cbBytesBuffered; /*total bytes received*/
	int cbBytesRcvd=0;
	int nRet=0, WSAErr;
	if(hDataFile!=hLastFile)
	{
		hLastFile=hDataFile;
		cbBytesBuffered=0;
	}
	/*read from server*/
	while(cbBytesBuffered<len)
	{
		nRet=recv(hDataSock,&(achInBuf[cbBytesBuffered]),len-cbBytesBuffered,0);
		if(nRet==SOCKET_ERROR)
		{
			WSAErr=WSAGetLastError();
			/*display significant errors*/
			if(WSAErr!=WSAEWOULDBLOCK)
				WSAperror(WSAErr,(LPSTR)"recv()");
			/*exit recv() loop on any error*/
			goto recv_end;
		}
		else if(nRet==0)
		{
			/*other side closed socket*/
			/*quit if server closed connection*/
			goto recv_end;
		}
		else
		{
			/*update byte counter and display*/
			lByteCOunt+=nRet;
			_ltoa(lByteCount,achTempBuf,10);
			SetDlgItemText(hWinMain,IDC_DATA_RATE,achTempBuf);
			cbBytesRcvd+=nRet; /*tally bytes read*/
			cbBytesBuffered+=nRet;
		}
	}
recv_end:
	if(!bToNul && ((cbBytesBuffered > (len-MTU_SIZE)) || ((nRet==SOCKET_ERROR) && WSAGetLastError()!=WSAEWOULDBLOCK)||(nRet==0)))
	{
		/*if over-buffer occurs, then write to data file*/
		nRet= _lwrite(hDataFile,achInBuf,cbBytesBuffered);
		if(nRet==HFILE_ERROR)
			MessageBox(hWinMain,"unable to write local file", "RecvData( ) Failed", MB_OK | MB_ICONASTERISK);
		cbBytesBuffered=0;
	}
	else if(bToNul)
		cbBytesBuffered=0;
	return(cbBytesRcvd);
} /*end of RecvData() */
