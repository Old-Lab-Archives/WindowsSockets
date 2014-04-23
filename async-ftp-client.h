#include<winsock.h>
#define CMD_SIZE 128
#define RPLY_SIZE MTU_SIZE
#define MAXNULPUT 1048576
/*FTP commands with arguments*/
#define CWD 1
#define DELE 2
#define PASS 3
#define PORT 4
#define RETR 5
#define STOR 6
#define TYPE 7
#define USER 8
/*FTP commands without arguments*/
#define ABOR 9
#define LIST 10
#define PWD 11
#define QUIT 12
/*FTP command strings*/
extern LPSTR aszFtpCmd[13];
/*Application states*/
#define NOT_CONNECTED 0
#define CTRLCONNECTED 2
#define DATACONNECTED 4
/*Global variables*/
extern char szAppName[];
extern BOOL nAppState; /*Application state*/
extern BOOL bToNul; /*get to NUL device file*/
extern BOOL bIOBeep; /*beep on FD_READ and FD_WRITE */
extern BOOL bDebug; /*debug output to WinDebug*/
extern BOOL bReAsync; /*call WSAAsyncSelect after accept*/
extern BOOL bLogFile; /*write commands and replies to log file*/
extern SOCKET hCtrlSock; /* FTP control socket */
extern SOCKET hLstnSock; /*listening data socket*/
extern SOCKET hDataSock; /*connected data socket*/
extern char szHost[MAXHOSTNAME]; /*remote host name or address*/
extern char szUser[MAXUSERNAME]; /*user ID*/
extern char szPwrd[MAXPASSWORD]; /*user password*/
extern SOCKADDR_IN stCLclName; /*Control socket name (local) */
extern SOCKADDR_IN stCRmtName; /*control socket name (Remote) */
extern SOCKADDR_IN stDLclName; /*data socket name (local client) */
extern SOCKADDR_IN stDRmtName; /*data socket name (remote client)*/
extern char achInBuf[INPUT_SIZE]; /*network input data buffer*/
extern char achOutBuf[INPUT_SIZE]; /*netwotk output buffer*/
extern char szFtpRply[RPLY_SIZE]; /*FTP reply (input) buffer*/
extern char szDataFile[MAXFILENAME]; /*file name*/
extern char szFtpCmd[CMD_SIZE]; /*FTP Command buffer*/
extern char achRplyBuf[BUF_SIZE]; /*reply display buffer*/
typedef struct stFtpCmd
{
	int nFtpCmd; /*FTP command value*/
	char szFtpParm[CMD_SIZE]; /*FTP parameter string*/
}
FTPCMD;
#define MAX_CMDS 4
/*first one (index=0) is awaiting a reply,
second one (index=1) is next to be sent */
extern FTPCMD astFtpCmd[MAX_CMDS]; /*FTP command queue*/
extern int nQLen; /*number entries in FTP command queue*/
extern int nFtpRplyCode; /*FTP reply code from server*/
extern int iNextRply; /*index to next reply string*/
extern int iLastRply;
extern HFILE hDataFile; /*file handle for open data file*/
extern LONG lStartTime; /*start time for data transfer*/
extern LONG lByteCount;
extern char szLogFile[]; /*FTP command and reply log file*/
extern HFILE hLogFile;
/*Function prototypes*/
BOOL CALLBACK Dlg_Main (HWND,UINT,UINT,LPARAM); /*Dialog procedures*/
BOOL CALLBACK Dlg_Login (HWND,UINT,UINT,LPARAM);
BOOL CALLBACK Dlg_File (HWND,UINT,UINT,LPARAM);
BOOL CALLBACK Dlg_Options (HWND,UINT,UINT,LPARAM);
BOOL CALLBACK Dlg_About (HWND,UINT,UINT,LPARAM);
SOCKET InitCtrlConn(PSOCKADDR_IN,HWND,u_int); /*control connection*/
BOOL QueueFtpCmd(int,LPSTR);
int SendFtpCmd(void);
void AbortFtpCmd(void);
int RecvFtpRply(SOCKET,LPSTR,int);
void ProcessFtpRply(LPSTR,int);
SOCKET InitDataConn(PSOCKADDR_IN,HWND,u_int); /*data connection*/
SOCKET AcceptDataConn(SOCKET,PSOCKADDR_IN);
long SendData(SOCKET*,HFILE,int);
int RecvData(SOCKET,HFILE,LPSTR,int);
void EndData(void);
int CloseConn(SOCKET*,LPSTR,int,HWND);
void not_connected(void); /*utility function*/
HFILE CreateLclFile(LPSTR);
/*end of async-ftp-client.h */
