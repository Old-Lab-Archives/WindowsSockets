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
