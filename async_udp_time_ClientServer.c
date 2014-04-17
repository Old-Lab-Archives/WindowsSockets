#include<WSA_Xtra.h>
#include<windows.h>
#include<winsockx.h>
#include<string.h>
#include<time.h>
#include<resource.h>
#include<windowsx.h>
/*Timeout ID and period*/
#define TIMEOUT_ID 1
#define TIMEOUT_PERIOD 30000
#define BUF_SIZE 1024
#define ERR_SIZE 512
/*global variables*/
WSADATA stWSADATA; /*WinSock DLL info*/
char szAppName[]="AU_Time";
BOOL bReceiving=FALSE; /*state flag*/
SOCKET hSock=INVALID_SOCKET; /*socket handle*/
char szHost[MAXHOST_NAME]={0}; /*remote host (name or address) */
SOCKADDR_IN stLclName; /*local socket name (address and port) */
SOCKADDR_IN stRmtName; /*remote socket name (address and port) */
char achInBuf[BUF_SIZE]; /*input buffer*/
char achOutBuf[BUF_SIZE]; /*output buffer*/
BOOL bBroadcast=FALSE; /*broadcast-enabled flag*/
HWND hwndMain; /*Main window handle*/
HINSTANCE hInst; /*Instance handle*/
/*---Funtion prototypes---*/
LONG FAR PASCAL WndProc(HWND,UINT,WPARAM,LPARAM);
BOOL FAR PASCAL AboutDlgProc(HWND, UINT, UINT, LONG);
BOOL FAR PASCAL DestDlgProc(HWND, UINT, UINT, LONG);
/* Function: WinMain()
Initialize and start the message loop*/
int PASCAL WinMain(HANDLE hInstance, HANDLE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	MSG msg;
	int nRet;
	WNDCLASS wndclass;
	lpszCmdLine=lpszCmdLine; /*avoid warning*/
		hInst=hInstance; /*save instance handle*/
		if(hPrevInstance)
		{
			wndclass.style=CS_HREDRAW | CS_VREDRAW;
			wndclass.lpfnWndProc=WndProc;
			wndclass.cbClsExtra=0;
			wndclass.cbWndExtra=0;
			wndclass.hInstance=hInstance;
			wndclass.hIcon=LoadIcon(hInst,MAKEINTRESOURCE(AU_TIME));
			wndclass.hCursor=LoadCursor(NULL,IDC_ARROW);
			wndclass.hbrBackground=COLOR_WINDOW+1;
			wndclass.lpszMenuName=MAKEINTRESOURCE(AU_TIME);
			wndclass.lpszClassName=szAppName;
			if(!RegisterClass(&wndclass))
			{
				return(0);
			}
		}
		hwndMain=CreateWindow(szAppName, "Daytime client and server", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, NULL, NULL, hInstance, NULL);
		if(!hwndMain)
			return 0;
		/* Initialize WinSock DLL */
		nRet=WSAStartup(WSA_VERSION, &stWSAData);
		/* if WSAStartup() returns error*/
		if(nRet!=0)
		{
			WSAperror(nRet, "WSAStartup()");
		}
		else
		{
			ShowWindow(hwndMain, nCmdShow); /*display window*/
			UpdateWindow(hwndMain);
			while(GetMessage(&msg,NULL,0,0))
			{
				/*main message loop*/
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			/*release Winsock DLL */
			if(nRet==SOCKET_ERROR)
				WSAperror(WSAGetLastError( ), "WSACleanup( )");
		}
		/*return resource explicitly*/
		UnregisterClass(szAppName, hInstance);
		return msg.wParam;
}
/*end of WinMain() */
/* Function: WndProc()
Process application messages for main window*/
LONG FAR PASCAL WndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM,lParam)
{
	FARPROC lpfnProc;
	int nAddrSize=sizeof(SOCKADDR);
	WORD WSAEvent, WSAErr;
	HMENU hMenu;
	int nRet;
	switch(msg)
	{
	case WSA_ASYNC:
		WSAEvent=WSAGETSELECTEVENT(lParam);
		WSAErr=WSAGETSELECTERROR(lParam);
		if(WSAErr)
		{
			/*error in async notification message*/
			WSAperror(WSAErr,"FD_READ");
		}
		switch(WSAEvent)
		{
		case FD_READ:
			/*receive the available data*/
			nRet=recvfrom(hSock,(char FAR *)achInBuf,BUF_SIZE,0,(struct sockaddr *)&stRmtName,&nAddrSize);
			/*display error if 'receive' failed*/
			if((nRet==SOCKET_ERROR)&&(WSAErr))
			{
				WSAperror(WSAErr,"recvfrom( )");
				break;
			}
			if(bReceiving) {
				achInbuf[nRet-2]=0; /*remove CR/LF */
			/*display the data received*/
			wsprintf((LPSTR)achOutBuf, "%s : %s", inet_ntoa(stRmtName.sin_addr),(LPSTR)achInBuf);
			MessageBox(hwnd,(LPSTR)achOutBuf,"Daytime",MB_OK|MB_ICONASTERISK);
			/*remove the timeout alert*/
			KillTimer(hwnd,TIMEOUT_ID);
			/*reset socket state*/
			bReceiving=FALSE;
		}
			else
			{
				time_t stTime;
				time(&stTime);
				wsprintf(achOutBuf,"%s",ctime(&stTime));
				nRet=sendto(hSock,achOutbuf,strlen(achOutBuf),0,(LPSOCKADDR)&stRmtName,sizeof(SOCKADDR));
				if(nRet==SOCKET_ERROR)
					WSAperror(WSAGetLastError( ), "sendto( )");
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
		case IDM_OPEN:
			if(hSock!=INVALID_SOCKET)
			{
				nRet=closesocket(hSock);
				hSock=INVALID_SOCKET;
			}
			if(nRet==SOCKET_ERROR)
				WSAperror(WSAGetLastError( ),"socket( )");
			/*get UDP socket*/
			hSocket=socket(AF_INET,SOCK_DGRAM,0);
			if(hSock==INVALID_SOCKET)
			{
				WSAperror(WSAGetLastError( ), "socket( )");
			}
			else
			{
				int okay=TRUE;
				/*request async notification for data arrival*/
				nRet=WSAAsyncSelect(hSock,hwnd,WSA_ASYNC,FD_READ);
				if(nRet==SOCKET_ERROR)
				{
					WSAperror(WSAGetLastError( ), "WSAAsyncSelect( )");
					okay=FALSE;
				}
				/*name the socket to receive requests as a server*/
				stlclName.sin_family=PF_INET;
				stLclName.sin_port=htons(IPPORT_DAYTIME);
				stLclName.sin_addr.sin_addr=INADDR_ANY;
				nRet=bind(hSock,(LPSOCKADDR)&stLclName,sizeof(struct sockaddr));
				if(nRet==SOCKET_ERROR)
				{
					WSAperror(WSAGetLastError( ), "bind( )");
					okay=FALSE;
				}
				if(ok)
				{
					wsprintf((LPSTR)achOutBuf,"Socket %d,named and registered for FD_READ",hSock);
					MessageBox(hwnd,(LPSTR)achOutBuf,"Ready to send or receive",MB_OK | MB_ICONASTERISK);
				}
			}
			break;
			/*note: the SendTo menu command sends the IDM_SENDTO message to AU_TIME*/
		case IDM_SENDTO:
			/*creating dialog box to prompt for destination host */
			lpfnProc=MakeProcInstance((FARPROC)DestDlgProc,hInst);
			nRet=DialogBox(hInst,"DESTINATIONLG",hwndMain,lpfnProc);
			FreeProcInstance((FARPROC) lpfnProc);
			/*checking destination address*/
			stRmtName.sin_addr.s_addr=GetAddr((LPSTR)szHost);
			if(stRmtName.sin_addr.s_addr==INADDR_ANY)
			{
				if(nRet!=-1)
					MessageBox(hwnd,"Need a destination host to send to","unable to connect!",MB_OK | MB_ICONASTERISK);
			}
			else
			{
				if(!SetTimer(hwnd,TIMEOUT_ID,TIMEOUT_PERIOD,NULL))
					MessageBox(hwnd,"Set timer failed","Error", MB_OK | MB_ICONASTERISK);
				/*set socket state... for a response as a client */
				bReceiving=TRUE;
				/*send a dummy datagram to daytime port..to request daytime response*/
				stRmtName.sin_family=PF_INET;
				stRmtName.sin_port=htons(IPPORT_DAYTIME);
				nRet=sendto(hSock,(char FAR *)achOutBuf,1,0,(LPSOCKADDR)&stRmtName,sizeof(SOCKADDR));
				if(nRet==SOCKET_ERROR)
					WSAperror(WSAGetLastError( ),"sendto( )");
			}
			break;
			/*the options menu allows the user to enable or disable broadcasts which sends IDM_BROADCAST message to AU_TIME */
		case IDM_BROADCAST:
			/* calls setsockopt() SO_BROADCAST to enable or disable*/
			hMenu=GetMenu(hwnd);
			bBroadcast=!CheckMenuItem(hMenu,IDM_BROADCAST,(bBroadcast ? MF_UNCHECKED : MF_CHECKED));
			nRet=setsockopt(hSock,SOL_SOCKET,SO_BROADCAST,(LPSTR)&bBroadcast,sizeof(BOOL));
			if(nRet==SOCKET_ERROR)
			WSAperror(WSAGetLastError( ),"setsockopt( )");
			break;
		case IDM_ABOUT:
			lpfnProc=MakeProcInstance((FARPROC)DlgAbout,hInst);
			DialogBox (hInst,"About",hwnd,lpfnProc);
			FreeProcInstance((FARPROC) lpfnProc);
			break;
		case IDM_EXIT:
			PostMessage(hwnd,WM_CLOSE,0,0L);
			break;
		default:
			return(DefWindowProc(hwnd,msg,wParam,lParam));
		} /*end of case WM_COMMAND*/
		break;
	case WM_TIMER:
		/*timeout occured*/
		bReceiving=FALSE; /*reset state*/
		KillTimer(hwnd,TIMEOUT_ID); /*release timer*/
		MessageBox(hwnd,"no response from daytime server", "Timeout", MB_OK | MB_ICONASTERISK);
		break;
	case WM_QUERYENDSESSION:
	case WM_CLOSE:
		/*closing the socket...bye bye */
		if(!SendMessage(hwnd,WM_COMMAND,IDM_CLOSE,1L))
			DestroyWindow(hwnd);
		/*release timer if active*/
		if(bReceiving)
			KillTimer(hwndMain,TIMEOUT_ID);
		break;
	case WM_CREATE:
		/*center dialog box*/
		CenterWnd(hwnd, NULL, TRUE);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return(DefWindowProc(hwnd,msg,wParam,lParam));
		}
		/*end of switch(msg)*/
		return 0;
		}
		/*end of WndProc() */
		/*--- Function: DestDlgProc()
		Prompt user for destination host */
		BOOL FAR PASCAL DestDlgProc
		{
			HWND hDlg;
			UINT msg;
			UINT wParam;
			LPARAM lParam;
			{
				static int wRet, nOptName, nOptVal, nOptLen, nOptIDC, nLevel, WSAErr;
				static HANDLE hwnd, hInst;
				static struct linger stLinger;
				switch(msg)
				{
				case WM_INITDIALOG:
					/*passing parameters*/
					if(lParam)
					{
						hInst=LOWORD(lParam);
						hwnd=HIWORD(lParam);
					}
					/*set display values*/
					SetDlgItemText(hDlg,IDC_DESTADDR,szHost);
					SetFocus(GetDlgItem(hDlg,IDC_COMBO1));
					/*center dialog box*/
					CenterWnd(hDlg, hwndMain, TRUE);
					return FALSE;
				case WM_COMMAND:
					switch(wParam)
					{
					case IDOK:
						GetDlgItemText(hDlg,IDC_DESTADDR,szHost,MAXHOST NAME);
						EndDialog(hDlg, TRUE);
					case IDCANCEL:
						EndDialog(hDlg, -1);
						break;
					}
					return(TRUE);
				}
				return(FALSE);
			} /*end DestDlgProc() */
