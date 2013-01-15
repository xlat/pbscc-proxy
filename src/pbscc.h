/*
 * Copyright 2010 Dmitry Y Lukyanov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <windows.h>
#include <stdio.h>
#include "scc.h"
#include "mstring.h"
#include "svninfo.h"

#define PBSCC_REVLEN  100
#define PBSCC_MSGLEN  2000
#define MAXFULLPATH 4000
#define PBSCC_HSTCNT 12

#define PBSCC_UID  30


#define WM_MANAGEOK		WM_USER+2
#define WM_COMMENTHST	WM_USER+3
//#define WM_LOADDDB      WM_USER+4
#define UM_SETFOCUS     WM_USER+5

//approximately sructure of the second parameter of SccQueryInfoEx callback function
typedef struct {
	DWORD        cb;      //structure size?
	DWORD        dw1;     //unknown = 0
	DWORD        status;  //object status? always = 1 
	LPSTR        object;  //object (full path)
	LPSTR        version; //object version
	DWORD        dw2;     //unknown = 0
	DWORD        dw3;     //unknown = 0
	DWORD        dw4;     //unknown = 0
	DWORD        dw5;     //unknown = 0
}INFOEXCALLBACKPARM;

typedef int  (*INFOEXCALLBACK)(LPVOID cbParm,INFOEXCALLBACKPARM * parm);

SCCEXTERNC SCCRTN EXTFUN SccQueryInfoEx(LPVOID pContext, 
			LONG nFiles, 
			LPCSTR* lpFileNames, 
			LPLONG lpStatus,
			INFOEXCALLBACK cbFunc,  //callback function to notify PB about version for each object
			LPVOID cbParm           //the reference to PB internal structure (used in callback function as first parm)
		);


typedef struct {
	CHAR          lpCallerName[SCC_PRJPATH_LEN+1];
	CHAR          lpProjName[SCC_PRJPATH_LEN+1]; //path to the SVN folder
	int           cbProjName;       //the length of the project name
	CHAR          lpProjPath[SCC_PRJPATH_LEN+1]; //local project path of PB
	int           cbProjPath;       //the lenght of the project path
	CHAR          lpUser[SCC_USER_LEN+1];
	mstring*      comment;
	LPTEXTOUTPROC lpOutProc;        //the output client procedure
	DWORD         dwLastUpdateTime; //the time(tick) of the last update operation
	DWORD         dwLastCommitTime; //the time(tick) of the last commit operation
	DWORD         dwLastGetTime;    //the time(tick) of the last SccGet operation ended
	DWORD         dwLastScanTime;   //the time of last scan operation
	bool          isLastAddRemove;  //contains true if last operation was add or remove
//	FILE          *fddb;
	CHAR          PBVersion[MAX_PATH+1]; //the version of the powerbuilder
	CHAR          PBTarget[MAXFULLPATH]; //the path to the local target file
	//the information for which files we need the comment
	LONG          nFiles; 
	LPCSTR        *lpFileNames;
	SCCCOMMAND    eSCCCommand;
	CHAR          lpTargetsTmp[MAXFULLPATH]; //the path where this context will store target filenames
	CHAR          lpOutTmp[MAXFULLPATH]; //the path where this context will store svn stdout
	CHAR          lpErrTmp[MAXFULLPATH]; //the path where this context will store svn stderr
	CHAR          lpMsgTmp[MAXFULLPATH]; //the path where this context will store user message
//	bool          noDDB; //is true if no ddb is set in registry
//	bool          doLock; //info from registry: shows if we want to lock on checkout.
	int           lockStrategy; //scc.ini: shows if we want to manage locks.
	bool          messagePrefix; //scc.ini: if we want to display predefined prefix on commit
	unsigned long cacheTtlMs;  //info from registry: time to live for cache in milliseconds
	mstring*      pipeErr;     //here we are storing stderr of the child process
	CHAR          uid[PBSCC_UID+1];
	CHAR          pwd[PBSCC_UID+1];
	HWND          parent;
	svninfo       *svni;
	CHAR          svnwd[SCC_USER_LEN];  //svn work directory. by default ".svn"
	CHAR          svnpucmd[1024];       //postupdate command
	DWORD         svndiff_method;       //0 for diff, 1 for Merge
	DWORD         svndiff_delay;        //force a delay while svndiff
	int           exportEncode;           //recode file when copy into svn workcopy
}THECONTEXT;

#define LOCKSTRATEGY_LOCK  1
#define LOCKSTRATEGY_PROP  2



#define DELAYFORNEWCOMMENT 2000
#define PBSCC_REGPATH "SOFTWARE\\FM2i\\PBSCC Proxy"
#define PBSCC_REGKEY HKEY_LOCAL_MACHINE


#define EENCODE_NONE	0
#define EENCODE_UTF8	1


void log(const char* szFmt,...);
boolean logEnabled();
bool ShowSysError(char*info,int err=0);
bool _loginscc(THECONTEXT*ctx);
void _msg(THECONTEXT*ctx,char * s);
extern HINSTANCE	hInstance;
bool ScanWC(THECONTEXT* ctx,bool force);
BOOL _copyfile(THECONTEXT *ctx,const char*src,char*dst);



