#include<windows.h>
#define MTU_SIZE 1460
#define INPUT_SIZE 8192
#define BUF_SIZE 1024
#define ERR_SIZE 512
#define MAXUSERNAME 64
#define MAXPASSWORD 32
#define MAXFILENAME 64
/*async notification message*/
#define WSA_ASYNC WM_USER+1
/*WinSock DLL info*/
extern WSADATA stWSADATA;
/*for error messages*/
extern char *aszWSAEvent[7];
/*screen I/O data buffer*/
extern char achTempBuf[BUF_SIZE];
/*temporary work file name*/
extern char szTempFile[10];
/*main window handle*/
extern HWND hWinMain;
/*instance handle*/
extern HINSTANCE hInst;
/*---Library function prototypes---*/
void CenterWnd(HWND,HWND,BOOL);
int CloseConn(SOCKET, LPSTR, int, HWND);
HFILE CreateLclFile(LPSTR);
BOOL CALLBACK Dlg_About(HWND, UINT, UINT, LPARAM);
u_long GetAddr(LPSTR);
int GetBuf(SOCKET, int, int);
LONG GetHostID(void);
BOOL GetLclDir(LPSTR szTempFile);
u_short GetPort(LPSTR);
void WSAperror(int, LPSTR);
int WSAErrStr(int, LPSTR);
