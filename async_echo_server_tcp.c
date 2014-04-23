#include "winsock.h"
#include "winsockx.h"
#include "WSA_Xtra.h"
#include "resource.h"
#include<windows.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>
#include<stdio.h>
#include<dos.h>
#include<direct.h>
#include<windowsx.h>
#define STRICT
/*global data*/
char szAppName[]="as_echo";
SOCKET hLstnSock=INVALID_SOCKET; /*listening socket*/
SOCKADDR_IN atLclName; /*local address and port number*/
char szHost_name[MAXHOST_NAME]={0}; /*local host name*/
int iActiveConns; /*number of active connections*/
long lByteCount; /*total bytes read*/
int iTotalConns; /*Connections closed so far*/
typedef struct stConnData
{
	SOCKET hSock; /*connection socket*/
	SOCKADDR_IN stRmtName; /*remote host address and port*/
	LONG lStartTime; /*time of connect*/
	BOOL bReadPending; /*deferred read flag*/
	int iBytesRcvd; /*data currently buffered*/
	int iBytesSent; /*data sent from buffer*/
	long lByteCount; /*total bytes received*/
	char achIOBuf[INPUT_SIZE]; /*network I/O data buffer*/
	struct stConnData FAR*lpstNext; /*pointer to next record*/
}
CONNDATA, *PCONNDATA, FAR *LPCONNDATA;
LPCONNDATA lpstConnHead=0; /*head of the list*/
char szLogFile[]="as_echo.log"; /*connection log file*/
HFILE hLogFile=HFILE_ERROR;
BOOL bReAsync=TRUE;
/* Function Prototypes */
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
BOOL CALLBACK Dlg_Main(HWND, UINT, UINT, LPARAM);
BOOL CALLBACK Dlg_About(HWND, UINT, UINT, LPARAM);
BOOL InitLstnSock(int, PSOCKADDR_IN, HWND, u_int);
SOCKET AcceptConn(SOCKET, PSOCKADDR_IN);
long SendData(SOCKET, LPSTR, int);
int RecvData(SOCKET, LPSTR, int);
void DoStats(long, long, LPCONNDATA);
LPCONNDATA NewConn(SOCKET, PSOCKADDR_IN);
LPCONNDATA FindConn(SOCKET);
void RemoveConn(LPCONNDATA);
/*Function: WinMain()
Initialize and start message loop */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	int nRet;
	lpszCmdLine=lpszCmdLine; /* avoid warning */
	hPrevInstance=hPrevInstance;
	nCmdShow=nCmdShow;
	hInst=hInstance; /* save instance handle */
	/* Initialize WinSock DLL */
	nRet=WSAStartup(WSA_VERSION, &stWSAData);
	/* WSAStartup() returns error value if failed (0 on success)*/
	if(nRet!=0)
	{
		WSAperror(nRet, "WSAStartup()");
	}
	else
	{
		DialogBox(hInst, MAKEINTRESOURCE(AS_ECHO),NULL,Dlg_Main);
		/*Release WinSock DLL*/
		nRet=WSACleanup();
		if(nRet==SOCKET_ERROR)
			WSAperror(WSAGetLastError(),"WSACleanup()");
	}
	return(0);
} /*end of WinMain()*/
/* Function: Dlg_Main()
Precess Windows Message */
BOOL CALLBACK Dlg_Main(HWND hDlg, UINT msg, UINT wParam, LPARAM lParam)
{
	LPCONNDATA lpstConn; /*work pointer*/
	WORD WSAEvent, WSAErr;
	SOCKADDR_IN stRmtName;
	SOCKET hSock;
	struct hostent *lpHost;
	BOOL bRet=FALSE;
	int nRet, cbRcvd, cbSent;
	switch(msg)
	{
	case WSA_ASYNC:
		/*Async notification message handlers*/
		hSock=(SOCKET)wParam; /*socket*/
		WSAEvent=WSAGETSELECTEVENT(lParam); /*extract event*/
		WSAErr=WSAGETSELECTEVENT(lParam); /*extract error*/
		lpstConn=FindConn(hSock);
		/*Close connection on error (don't show error) */
		if(WSAErr && (hSock!=hInstSock))
		{
			PostMessage(hWinMain, WSA_ASYNC, (WPARAM)hSock, WSAMAKESELECTREPLY(FD_CLOSE,0));
			break;
		}
		switch(WSAEvent)
		{
		case FD_READ:
			if(lpstConn)
			{
				/*read data from socket and write it back*/
				cbRcvd=lpstConn->iBytesRcvd;
				if((INPUT_SIZE - cbRcvd) > 0)
				{
					lpstConn->bReadPending=FALSE;
					nRet=RecvData(hSock, (LPSTR)&(lpstConn->achIOBuf[cbRcvd]), INPUT_SIZE-cbRcvd);
					lpstConn->iBytesRcvd+=nRet;
					lpstConn->lByteCount+=nRet;
					lByteCount+=nRet;
					_ltoa(lByteCount, achTempBuf, 10);
					SetDlgItemText(hWinMain, IDC_BYTE_TOTAL, achTempBuf);
				}
				else
				{
					/* no fucking buffer now, so defer the net read */
					lpstConn->bReadPending=TRUE;
				}
			}
			/*fall through to write data back to client*/
			/*In response to FD_READ notifications, we call our own RecvData() function, evaluate the data received and update on-screen statistics*/
		case FD_WRITE:
			if(lpstConn && lpstConn->iBytesRcvd)
			{
				cbSent=lpstConn->iBytesSent;
				lpstConn->iBytesSent+=SendData(hSock,(LPSTR)&(lpstConn->achIOBuf[cbSent]),lpstConn->iBytesRcvd - cbSent);
				/*If everything sent which is received, then reset the counters*/
				if(lpstConn->iBytesSent==lpstConn->iBytesRcvd)
				{
					lpstConn->iBytesSent=0;
					lpstConn->iBytesRcvd=0;
				}
				/* if there is annoying read pending, then do it */
				if(lpstConn->bReadPending)
				{
					PostMessage(hWinMain, WSA_ASYNC, (WPARAM)hSock, WSAMAKESELECTREPLY(FD_READ,0));
				}
			}
			break;
			/*We use FD_WRITE only after a previous send() failed with WSAEWOULDBLOCK */
		case FD_ACCEPT:
			/*Accept the incoming the data connection request*/
			hSock=AcceptConn(hLstnSock, &stRmtName);
			if(hSock!=INVALID_SOCKET)
			{
				/*get a new socket structure*/
				lpstConn=NewConn(hSock, &stRmtName);
				if(!lpstConn)
				{
					/*Sample libraries added: --- available in winsockx.lib  and also, coded at bottom of this program ---
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
					CloseConn(hSock, (LPSTR)0, INPUT_SIZE, hWinMain);
				}
				else
				{
					iActiveConns++;
					SetDlgItemInt(hWinMain, IDC_CONN_ACTIVE, iActiveConns, FALSE);
				}
			}
			break;
		case FD_CLOSE: /*data connection closed*/
			if(hSock!=hLstnSock)
			{
				/*read any remaining data and close connection*/
				CloseConn(hSock, (LPSTR)0, INPUT_SIZE, hWinMain);
				if(lpstConn)
				{
					DoStats(lpstConn->lByteCount, lpstConn->lStartTime, lpstConn);
					RemoveConn(lpstConn);
					iTotalConns++;
					SetDlgItemInt(hWinMain, IDC_CONN_TOTAL, iTotalConns, FALSE);
					iActiveConns--;
					SetDlgItemInt(hWinMain, IDC_CONN_ACTIVE, iActiveConns, FALSE);
				}
			}
			break;
		default:
			break;
		}
		/*end of switch(WSAEvent)*/
		break;
	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hDlg, Dlg_About);
			break;
		case WM_DESTROY:
		case IDC_EXIT:
			/*close listening socket*/
			if(hLstnSock!=INVALID_SOCKET)
				closesocket(hLstnSock);
			/*close all active connections*/
			for(lpstConn=lpstConnHead;lpstConn;lpstConn=lpstConn->lpstNext)
			{
				CloseConn(lpstConn->hSock,(LPSTR)0,INPUT_SIZE,hWinMain);
				RemoveConn(lpstConn);
			}
			/*write final stats and close log file */
			if(hLogFile!=HFILE_ERROR)
			{
				wsprintf(achTempBuf, "Final Total: Bytes Received: %lu, Connections: %d \n", lByteCount, iTotalConns);
				_lwrite(hLogFile, achTempBuf, strlen(achTempBuf));
				_lclose(hLogFile);
			}
			EndDialog(hDlg, msg);
			bRet=TRUE;
			break;
		default:
			break;
		}
		/*end of WM_COMMAND */
		break;
	case WM_INITDIALOG:
		hWinMain=hDlg; /*save main window handle */
		/*get socket listener*/
		hLstnSock=InitLstnSock(IPPORT_ECHO, &stLclName, hWinName, WSA_ASYNC);
		/*get local info for display*/
		stLclName.sin_addr.s_addr=GetHostID();
		gethostname(szHost name, MAXHOST_NAME);
		wsprintf(achTempBuf, "Local host: %s (%s)", inet_ntoa(stLclName.sin_addr), szHost_name[0] ? (LPSTR)szHost_name : (LPSTR)"<unknown>");
		SetDlgItemText(hWinMain,IDC_LOCAL_HOST,achTempBuf);
		/*assign an icon to dialog box*/
#ifndef WIN32
		SetClassWord(hDlg,GCW_HICON,(WORD)LoadIcon(hInst,MAKEINTRESOURCE(AS_ECHO)));
#else
		SetClassLong(hDlg,GCL_HICON,(LONG)LoadIcon((HINSTANCE) GetWindowLong(hDlg, GWL_HINSTANCE), __TEXT("AS_ECHO")));
#endif
		/*open log file "if" logging is enabled */
		hLogFile= _lcreat(szLogFile, 0);
		if(hLogFile==HFILE_ERROR)
		{
			MessageBox(hWinMain, "unable to open log file", "file error", MB_OK | MB_ICONASTERISK);
		}
		/*center dialog box*/
		CenterWnd(hDlg,NULL,TRUE);
		break;
	default:
		break;
		}
		/*end switch(msg) */
		return(bRet);
		}
		/*end of Dlg_Main()*/
		/*---Function: InitLstnSock()
		Get a stream socket and start listening for incoming connection requests */
		BOOL InitLstnSock(int iLstnPort, PSOCKADDR_IN pstSockName, HWND hwnd, u_int nAsyncMsg)
		{
			int nRet;
			SOCKET hLstnSock;
			int nLen=SOCKADDR_LEN;
			/*Get a TCP socket to use for data connection ~ listen*/
			hLstnSock=socket(AF_INET, SOCK_STREAM, 0);
			if(hLstnSock==INVALID_SOCKET)
			{
				WSAperror(WSAGetLastError(), "socket()");
			} else
			{
				/*request async notifications for most events*/
				nRet=WSAAsyncSelect(hLstnSock, hWnd, nAsyncMsg,(FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE));
				if(nRet==SOCKET_ERROR)
				{
					WSAperror(WSAGetLastError(),"WSAAsyncSelect()");
				}
				else
				{
					/*name the local socket with bind()*/
					pstSockName->sin_family=PF_INET;
					pstSockName->sin_port=htons(iLstnPort);
					nRet=bind(hLstnSock,(LPSOCKADDR)pstSockName,SOCKADDR_LEN);
					if(nRet==SOCKET_ERROR)
					{
						WSAperror(WSAGetLastError(), "listen()");
					}
				}
			}
			/* if we get an error, OMG! Then, we gotta problem. Clean the shit up! */
			if(nRet==SOCKET_ERROR)
			{
				closesocket(hLstnSock);
				hLstnSock=INVALID_SOCKET;
			}
		}
		return(hInstSock);
		} /*end of InitLstnSock() */
		/*--- Function: AcceptConn()
		In response to FD_ACCEPT, call accept() inorder to respond to connection request & establish connection*/
		SOCKET AcceptConn(SOCKET hLstnSock, PSOCKADDR_IN pstName)
		{
			SOCKET hNewSock;
			int nRet, nLen = SOCKADDR_LEN;
			hNewSock=accept(hLstnSock,(LPSOCKADDR)pstName,(LPINT)&nLen);
			if(hNewSock==SOCKET_ERROR)
			{
				int WSAErr = WSAGetLastError();
				if(WSAErr!=WSAEWOULDBLOCK)
					WSAperror(WSAErr,"accept");
			}
			else if(bReAsync)
			{
				nRet=WSAAsyncSelect(hNewSock,hWinMain,WSA_ASYNC,(FD_READ | FD_WRITE | FD_CLOSE));
				if(nRet==SOCKET_ERROR)
				{
					WSAperror(WSAGetLastError(),"WSAAsyncSelect()");
				}
				/*getting extra buffer space*/
				GetBuf(hNewSock, INPUT_SIZE, SO_RCVBUF);
				GetBuf(hNewSock, INPUT_SIZE, SO_SNDBUF);
			}
			return(hNewSock);
		} /*end AcceptConn() */
		/*--- Function: SendData()
		Send data from buffer provided
		*/
		long SendData(SOCKET hSock, LPSTR lpOutBuf, int cbTotalToSend)
		{
			int cbTotalSent=0;
			int cbLeftToSend = cbTotalToSend;
			int nRet, WSAErr;
			/*Sending data*/
			while(cbLeftToSend > 0)
			{
				/*Send data to client*/
				nRet=send(hSock,lpOutBuf+cbTotalSent,cbLeftToSend < MTU_SIZE ? cbLeftToSend : MTU_SIZE, 0);
				if(nRet==SOCKET_ERROR)
				{
					WSAErr=WSAGetLastError();
					/*display errors*/
					if(WSAErr!=WSAEWOULDBLOCK)
					{
						WSAperror(WSAErr, (LPSTR)"send()");
					}
					break;
				}
				else
				{
					/*update byte counter and display*/
					cbTotalSent+=nRet;
				}
				/*calculate the remaining ones =P */
				cbLeftToSend=cbTotalToSend - cbTotalSent;
			}
			return(cbTotalSent);
		} /*end of SendData() */
		/*--- Function:- RecvData()
		Receive data into buffer provided */
		int RecvData(SOCKET hSock, LPSTR lpInBuf, int cbTotalToRecv)
		{
			int cbTotalRcvd=0;
			int cbLeftToRecv=cbTotalToRecv;
			int nRet=0, WSAErr;
			/*read buffer from client*/
			while(cbLeftToRecv > 0)
			{
				nRet=recv(hSock,lpInBuf+cbTotalRcvd,cbLeftRecv,0);
				if(nRet==SOCKET_ERROR)
				{
					WSAErr=WSAGetLastError();
					/*display errors*/
					if(WSAErr!=WSAEWOULDBLOCK)
						WSAperror(WSAErr,(LPSTR)"recv()");
					/*exit recv() loop on any error*/
					break;
				}
				else if(nRet==0)
				{
					/*other side closed socket*/
					/*quit if server closed connection*/
					break;
				}
				else
				{
					/*update byte counter*/
					cbTotalRcvd+=nRet;
				}
				cbLeftToRecv=cbTotalToRecv - cbTotalRcvd;
			}
			return(cbTotalRcvd);
		} /*end RecvData() */
		/*---Function: DoStats()
		Display data transfer rate */
		void DoStats(long lByteCount, long lStartTime, LPCONNDATA lpstConn)
		{
			LONG dByteRate;
			LONG lMSecs;
			/*calculate data transfer rate and display*/
			lMSecs=(LONG) GetTickCount() - lStartTime;
			if(lMSecs <= 55)
				lMSecs=27;
			if(lByteCount>0L)
			{
				if(lpstConn)
				{
					wsprintf(achTempBuf,"<<< socket: %d disconnected from %s \n", lpstConn->hSock, inet_ntoa(lpstConn->stRmtName.sin_addr));
					_lwrite(hLogFile, achTempBuf, strlen(achTempBuf));
				}
				dByteRate=(lByteCount/lMSecs); /*Data rate*/
				wsprintf(achTempBuf, "%ld bytes in %ld.%ld seconds (%ld.%ld Kbytes/sec)\n",lByteCount,lMSecs/1000,lMSecs%1000,(dByte*1000)/1024,(dByteRate*1000)%1024);
				SetDlgItemText(hWinMain,IDC_DATA_RATE,achTempBuf);
				if(hLogFile!=HFILE_ERROR)
					_lwrite(hLogFile, achTempBuf, strlen(achTempBuf));
			}
		} /*end of DoStats() */
		/*---Function : NewConn()
		Create a new socket structure and put in list */
		LPCONNDATA NewConn(SOCKET hSock, PSOCKADDR_IN pstRmtName)
		{
			int nAddrSize=sizeof(SOCKADDR);
			LPCONNDATA lpstConnTmp, lpstConn=(LPCONNDATA)0;
			HLOCAL hSockData;
			/*allocate memory for new socket structure*/
			hSockData=LocalAlloc(LMEM_ZEROINIT,sizeof(SOCKDATA));
			if(hSockData!=0)
			{
				/*lock it and link it into the list*/
				lpstConn=LocalLock(hSockData);
				if(!lpstConnHead)
				{
					lpstConnHead=lpstConn;
				}
				else {
					for(lpstConnTmp=lpstConnHead;lpstConnTmp && lpstConnTmp->lpstNext;lpstConnTmp=lpstConnTmp->lpstNext);
					lpstConnTmp->lpstNext=lpstConn;
				}
				/*Initialize socket structure*/
				lpstConn->hSock=hSock;
				_fmemcpy((LPSTR)&&(lpstConn->stRmtName),(LPSTR)pstRmtName,sizeof(SOCKADDR));
				lpstConn->lStartTime=GetTickCount();
				/*Log the new connection*/
				if(hLogFile!=HFILE_ERROR)
				{
					wsprintf(achTempBuf,">>> socket: %d connected from %s \n", hSock, inet_ntoa(lpstConn->stRmtName.sin_addr));
					_lwrite(hLogFile,achTempBuf,strlen(achTempBuf));
				}
			}
			else
			{
				MessageBox(hWinMain, "unable to allocate memory for connectivity", "LocalAlloc() Error", MB_OK | MB_ICONASTERISK);
			}
			return(lpstConn);
		} /*end of NewConn() */
		/* Function:--- FindConn()
		Find socket structure for connection */
		LPCONNDATA FindConn(SOCKET hSock)
		{
			LPCONNDATA lpstConnTmp;
			for(lpstConnTmp=lpstConnHead;lpstConnTmp;lpstConnTmp=lpstConnTmp->lpstNext)
			{
				if(lpstConnTmp->hSock==hSock)
					break;
			}
			return(lpstConnTmp);
		} /*end of FindConn() */
		/*--- Function: RemoveConn()
		Free the memory for socket structure */
		void RemoveConn(LPCONNDATA lpstConn)
		{
			LPCONNDATA lpstConnTmp;
			HLOCAL hSock;
			if(lpstConn==lpstConnHead)
			{
				lpstConnHead=lpstConn->lpstNext;
			}
			else
			{
				for(lpstConnTmp=lpstConnHead;lpstConnTmp;lpstConnTmp=lpstConnTmp->lpstNext)
				{
					if(lpstConnTmp->lpstNext==lpstConn)
						lpstConnTmp->lpstNext=lpstConn->lpstNext;
				}
			}
			hSock=LocalHandle(lpstConn);
			LocalUnlock(hSock);
			LocalFree(hSock);
		} /*end of RemoveConn() */
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
