#include<windows.h>
#include<windowsx.h>
#include<winsock.h>
#include "WSA_Xtra.h"
#include "resource.h"
#include<string.h> /* for free memcpy()*/
#include<stdlib.h> /*for atoi()*/
#include "winsockx.h"
#define TIMEOUT_ID WM_USER+1
/*data structures*/
typedef struct TaskData
{
	HTASK hTask; /*task ID: Primary key*/
	int nRefCount; /*number of sockets owned by task*/
	struct TaskData *lpstNext; /*pointer to next entry in linked list*/
}
TASKDATA, *PTASKDATA, FAR *LPTASKDATA;
typedef struct ConnData
{
	SOCKET hSock; /*connection socket*/
	LPTASKDATA lpstTask; /*pointer to task structure*/
	HWND hwnd; /*handle of subclassed window*/
	SOCKADDR_IN stRmtName; /*remote host address and port*/
	int nTimeout; /*timeout*/
	DWORD lpfnWndProc; /*task's window procedure*/
	struct ConnData *lpstNext;
}
CONNDATA, *PCONNDATA, FAR *LPCONNDATA;
/*Global data*/
char szAppName[]="WinsockAPI_DLL";
HWND hWinMain;
HINSTANCE hInst;
WSADATA stWSAData;
LPCONNDATA lpstConnHead = 0L; /*head of connection data list*/
LPTASKDATA lpstTaskHead = 0L; /*head of task data list*/
/*Exported Function Prototypes*/
int WINAPI LibMain(HANDLE, WORD, WORD, LPSTR);
LONG CALLBACK SubclassProc(HWND, UINT, WAPARAM, LPARAM);
SOCKET WINAPI ConnectTCP(LPSTR, LPSTR);
int WINAPI SendData(SOCKET, LPSTR, int);
int WINAPI RecvData(SOCKET, LPSTR, int, int);
int WINAPI CloseData(SOCKET, LPSTR, int);
/*internal function prototypes*/
int DoSend(SOCKET, LPSTR, int, LPCONNDATA);
int DoRecv(SOCKET, LPSTR, int, LPCONNDATA);
LPCONNDATA NewConn(SOCKET, PSOCKADDR_IN);
LPCONNDATA FindConn(SOCKET, HWND);
void RemoveConn(LPCONNDATA);
LPTASKDATA NewTask(HTASK);
LPTASKDATA FindTask(HTASK);
void RemoveTask(LPTASKDATA);
/*Function:- LibMain() ---> DLL entry point --- */
int PASCAL LibMain(HANDLE hInstance, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCmdLine)
{
	lpszCmdLine = lpszCmdLine; /*avoid warnings*/
	wDataSeg = wDataSeg;
	wHeapSize = wHeapSize;
	hInst = hInstance; /*save instance handle*/
	return(1);
} /*end of LibMain()*/
/*Function: SubclassProc() --- Filter unwanted I/O messages from active window in each task while blocking operation completes */
LONG FAR PASCAL EXPORT SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPCONNDATA lpstConn; /*work pointer*/
	lpstConn = FindConn(0, hwnd); /*find our socket structure*/
	switch(msg)
	{
	case WM_QUIT:
		/*close this connection*/
		if(lpstConn)
		{
			CloseTCP(lastConn->hSock, (LPSTR)0, INPUT_SIZE);
			RemoveConn(lpstConn);
			/*release timer*/
			if(lpstConn->nTimeout)
				KillTimer(hwnd, TIMEOUT_ID);
		}
		break;
	case WM_CLOSE:
	case WM_TIMER:
		/*if timeout or close request, then cancel pending operation*/
		if(lpstConn && WSAIsBlocking())
			WSACancelBlockingCall();
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEACTIVATE:
	case WM_MOUSEMOVE:
	case WM_NCHITTEST:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCMBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMOUSEMOVE:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NEXTDLGCTL:
	case WM_RBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_SYSCHAR:
	case WM_SYSDEADCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		return(0L);
	default:
		break;
	} /*end of switch(msg)*/
	if(lpstConn)
	{
		return(CallWindowProc((WNDPROC)(lpstConn->lpfnWndProc),hwnd, msg, wParam, lParam));
	}
	else
	{
		return(0L);
	}
} /*end of SubclassProc() */
/*Function:--- ConnectTCP()
Get a TCP socket and connect to server */
SOCKET WINAPI ConnectTCP(LPSTR szDestination, LPSTR szService)
{
	int nRet;
	HTASK hTask;
	SOCKET hSock;
	LPTASKDATA lpstTask;
	LPCONNDATA lpstConn;
	SOCKADDR_IN stRmtName;
	hTask=GetCurrentTask(); /*task handle: for our records*/
	lpstTask=FindTask(hTask);
	if(lpstTask)
	{
		/*get TCP socket*/
		hSock=socket(AF_INET,SOCK_STREAM,0);
		if(hSock==INVALID_SOCKET)
		{
			WSAperror(WSAGetLastError()."socket()");
		}
		else
		{
			/*get destination address*/
			stRmtName.sin_addr.s_addr=GetAddr(szDestination);
			if(stRmtName.sin_addr.s_addr!=INADDR_NONE)
			{
				/*get destination port number*/
				stRmtName.sin_port=GetPort(szService);
				if(stRmtName.sin_port)
				{
					/*create a new socket structure*/
					lpstConn = NewConn(hSock,&stRmtName);
					if(lpstConn)
					{
						/*subclass the active window passed*/
						lpstConn->lpstTask=lpstTask;
						lpstConn->hwnd=GetActiveWindow();
						lpstConn->lpfnWndProc=GetWindowLong(lpstConn->hwnd,GWL_WNDPROC);
						SetWindowLong(lpstConn->hwnd,GWL_WNDPROC,(DWORD)SubclassProc);
						/*initiate non-blocking connect to server*/
						stRmtName.sin_family = PF_INET;
						nRet = connect(hSock,(LPSOCKADDR)&stRmtName,SOCKADDR_LEN);
						if(nRet==SOCKET_ERROR)
						{
							int WSAErr = WSAGetLastError();
							if(WSAErr!=WSAEINTR)
							{
								SetWindowLong(lpstConn->hwnd,GWL_WNDPROC,(DWORD)lpstConn->lpfnWndProc);
								WSAperror(WSAErr,"connect()");
								RemoveConn(lpstConn);
								closesocket(hSock);
								hSock=INVALID_SOCKET;
							}
						}
					}
					else
					{
						/*unable to create a connection structure*/
						closesocket(hSock);
						hSock = INVALID_SOCKET;
					}
				}
				else
				{
					/*unable to resolve destination port number*/
					closesocket(hSock);
					hSock = INVALID_SOCKET;
				}
			}
			else
			{
				/*unable to resolve destination address*/
				closesocket(hSock);
				hSock = INVALID_SOCKET;
			}
		}
		/*if failed, then clean up*/
		if(hSock==INVALID_SOCKET)
		{
			RemoveTask(lpstTask);
		}
		else if(lpstConn)
		{
			/*un-subclass active window before leaving*/
			SetWindowLong(lpstConn->hwnd, GWL_WNDPROC,(DWORD)lpstConn->LpfnWndProc);
		}
	}
	return(hSock);
} /*end of ConnectTCP()*/
/*Function:-- SendData()
Send data to socket from the buffer passed until the requested number of bytes is sent */
int WINAPI SendData(SOCKET hSock,LPSTR lpOutBuf,int cbTotalToSend)
{
	LPCONNDATA lpstConn;
	int cbTotalSent=0, cbSent;
	int nRet=SOCKET_ERROR; /*assume error*/
	lpstConn=FindConn(hSock,0);
	if(!lpstConn)
	{
		/*socket not found, so it's not valid*/
		WSASetLastError(WSAENOTSOCK);
	}
	else
	{
		/*subclass the window provided at connect to filter messages*/
		SetWindowLong(lpstConn->hwnd, GWL_WNDPROC, (DWORD)SubclassProc);
		while(((cbTotalToSend - cbTotalSent) > 0) && (lpstConn->hSock!=INVALID_SOCKET))
		{
			cbSent = DoSend(hSock, lpOutBuf+cbTotalSent, cbTotalToSend - cbTotalSent, lpstConn);
			if(cbSent != SOCKET_ERROR)
			{
				/*evaluate and quit the loop*/
				cbTotalSent += cbSent;
				if((cbTotalToSend - cbTotalSent) <= 0)
					break;
			}
			else
			{
				/*if send failed, return error*/
				cbTotalSent=SOCKET_ERROR;
			}
		}
		/*un-subclass active window before leaving*/
		SetWindowLong(lpstConn->hwnd, GWL_WNDPROC, (long)lpstConn->lpfnWndProc);
	}
	return(cbTotalSent);
} /*end of SendData() */
/*Function: RecvData()
Received data from socket into buffer passed until the requested number of bytes is received or timeout period is exceeded*/
int WINAPI RecvData(SOCKET hSock, LPSTR lpInBuf, int cbTotalToRecv, int nTimeout)
{
	LPCONNDATA lpstConn;
	int cbTotalRcvd=0, cbRcvd;
	int nRet=SOCKET_ERROR; /*assume error*/
	lpstConn=FindConn(hSock,0);
	if(!lpstConn)
	{
		/*socket not found and so no way of validity bro */
		WSASetLastError(WSAENOTSOCK);
	}
	else
	{
		/*subclass the active window to filter message traffic*/
		SetWindowLong(lpstConn->hwnd, GWL_WNDPROC, (DWORD)SubclassProc);
		/*set a timer if requested*/
		if(nTimeout)
		{
			lpstConn->nTimeout=nTimeout;
			SetTimer(hWinMain, TIMEOUT_ID, nTimeout, 0L);
		}
		while(((cbTotalToRecv - cbTotalRcvd) > 0) && (lpstConn->hSock!=INVALID_SOCKET))
		{
			cbRcvd=DoRecv(hSock, lpInBuf+cbTotalToRecv, cbTotalToRecv - cbTotalRcvd, lpstConn);
			if(cbRcvd!=SOCKET_ERROR)
			{
				/*Evaluate and quit if we've received the amount requested*/
				cbTotalRcvd+=cbRcvd;
				if((cbTotalToRecv - cbTotalRcvd) <= 0)
				{
					if(lpstConn->nTimeout)
					{
						/*reset timer*/
						KillTimer(lpstConn->hwnd, TIMEOUT_ID);
						break;
					}
					if(lpstConn->nTimeout)
					{
						/*reset timer*/
						SetTimer(hWinMain, TIMEOUT_ID, lpstConn->nTimeout, 0L);
					}
				}
				else
				{
					/*if receive failed, we return an error*/
					cbTotalRcvd=SOCKET_ERROR;
				}
			}
			/*unsubclass active window before leaving*/
			SetWindowLong(lpstConn->hwnd, GWL_WNDPROC, (long)lpstConn->lpfnWndProc);
			lpstConn->nTimeout=0; /*reset timer*/
		}
	}
		return (cbTotalRcvd);
} /*end of RecvData()*/
