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

#include "pbscc.h"
#include "res.h"
#include <shlobj.h>
#include <io.h>
#include <process.h>
#include <wininet.h>
#include "todo.h"
#include "easystr.h"
#include "entries.h"
#include "filecmp.h"
#include "filecpy.h"
#include "conproc.h"
#include "svninfo.h"
#include "svnstat.h"
#include "tmp\version.h"
#include "FCNTL.H"


HINSTANCE	hInstance;
FILE		*logFile=NULL;
CHAR		*gpSccName="PBSCC Proxy";

HWND		consoleHwnd=NULL;

void log(const char* szFmt,...) {
	if(logFile){
		va_list args;
		va_start(args, szFmt);
		vfprintf(logFile, szFmt, args);
		fflush(logFile);
		va_end(args);
	}
}

boolean logEnabled(){
	return logFile?1:0;
}

bool ShowSysError(char*info,int err){
	LPVOID lpMsgBuf;
	if(err==0)err=GetLastError();
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPTSTR) &lpMsgBuf, 0, NULL );
	log("System Error: %s : %s\n",info,lpMsgBuf);
	LocalFree( lpMsgBuf );
	return false;
}


BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD ul_reason_for_call, void* lpReserved) {
	switch (ul_reason_for_call){
		case DLL_PROCESS_ATTACH:{
			hInstance=hInst;
			HKEY rkey;
			DWORD type=REG_SZ;
			char buf[1024];
			DWORD buflen=sizeof(buf);
			if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
				if(RegQueryValueEx(rkey,"log.path",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
					if(!logFile)logFile=fopen(buf,"at");
					log("\n\n---------------------------------------------------------\n"
						"DllMain DLL_PROCESS_ATTACH hInst=%X\n",hInst);
					log("version=%s\n",PROJECT_VER);
					log("compile.date=%s\n",PROJECT_DATE);
				}
				RegCloseKey(rkey);
			}
			
			break;
		}
		case DLL_PROCESS_DETACH:{
			if(logFile){
				log("DllMain DLL_PROCESS_DETACH\n");
				fclose(logFile);
				logFile=NULL;
			}
			break;
		}
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
} 

//substitutes the local file path by project path
//returns reference to the internal buffer with substituted path
char * _subst(THECONTEXT*ctx,const char * file){
	static mstring subst=mstring();
	
	subst.set(ctx->lpProjName);
	subst.append(file+ctx->cbProjPath);
	return subst.c_str();
}

char * _rootfilename(THECONTEXT*ctx, const char * file){
	static mstring subst=mstring();	
	subst.set(file+ctx->cbProjPath + 1);
	return subst.c_str();
}

char * _dupsrc(THECONTEXT*ctx, char * prefix, const char * file){
	static mstring subst=mstring();
	subst.set(prefix);
	subst.append(file+ctx->cbProjPath+1);
	return subst.c_str();
}

//send to app message text
//splitting text by carrage return
void _msg(THECONTEXT*ctx,char * s){
	if(ctx->lpOutProc) {
		ctx->lpOutProc(s,strlen(s));
	}
}


//TODO: THERE is a bug. If in we have a multiline property, this will not work
BOOL GetProperty(char*fname,char*pname,char*pvalue,int pvlen){
	char c;
	int len;
	bool b=false;
	char buf[1000];
	FILE*f=fopen(fname,"rt");
	pvalue[0]=0;
	if(f){
		while(!b){
			if(fscanf(f,"%c %i\n",&c,&len)!=2)break;
			if(!fgets(buf,sizeof(buf),f))break;
			if(!strcmp(rtrim(buf),pname))b=true;
			if(fscanf(f,"%c %i\n",&c,&len)!=2)break;
			if(!fgets(buf,sizeof(buf),f))break;
		}
		if(b){
			strncpy(pvalue,buf,pvlen);
			pvalue[pvlen-1]=0;
			rtrim(pvalue);
		}
		fclose(f);
	}
	return b;
}


/** new work-copy scan callback */
bool _entries_scanwc_callback(SVNENTRY*e,void*udata) {
	THECONTEXT* ctx=(THECONTEXT*)udata;
	if( !strcmp(e->kind,"dir") && !e->name[0] ){
		//this triggered once for one scan
		//scan subdirectories
		WIN32_FIND_DATA ffd;
		HANDLE ffh;
		mstring ffp=mstring(e->wcpath);
		ffp.addPath("*");
		
		ffh=FindFirstFile(  ffp.c_str() ,&ffd);
		if(ffh!=INVALID_HANDLE_VALUE){
			do{
				if(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY && strcmp(ffd.cFileName,ctx->svnwd) && strcmp(ffd.cFileName,".") && strcmp(ffd.cFileName,"..")){
					mstring subdir=mstring(e->wcpath);
					subdir.addPath(ffd.cFileName);
					entries_scan(subdir.c_str(), &_entries_scanwc_callback, udata , ctx->svnwd);
				}
			}while(FindNextFile(ffh,&ffd));
			FindClose(ffh);
		}
	}else if( !strcmp(e->kind,"file") ){
		//isOwner calculated in a different way for different lock strategies.
		bool isOwner=false;
		//by default local locks are filled from entries
		//only if it's not filled we try to get it in another way
		if( ctx->lockStrategy & LOCKSTRATEGY_PROP ){
			//let's get lockby property
			static mstring propPath=mstring();
			//build properties file path
			propPath.set(e->wcpath)->addPath(ctx->svnwd)->addPath("prop-base")->addPath(e->name)->append(".svn-base");
			GetProperty(propPath.c_str(),"lockby",e->lockowner,ES_SIMPLE_LEN);
			isOwner=(! stricmp(e->lockowner,ctx->lpUser) );
		}
		//add information into in-memory cache
		ctx->svni->add(ctx->lpProjName,e->wcpath,e->name,e->revision,e->lockowner,isOwner);
	}
	
	return true;
}

BOOL __sccupdate(THECONTEXT*ctx){
	if(GetTickCount() - ctx->dwLastUpdateTime > DELAYFORNEWCOMMENT) {
		log("Update repository.\n");
		mstring pipeOut=mstring();
		if(!_execscc(ctx,&pipeOut,"svn update --non-interactive --trust-server-cert \"%s\"",ctx->lpProjName))return false;
		if(pipeOut.match("^[CG] ")){
			_msg(ctx,"Conflict during updating repository.");
			_msg(ctx,"Please resolve all conflicts manually to continue work.");
		}
		if(ctx->svnpucmd && strlen(ctx->svnpucmd)){
			//"TortoiseProc.exe /Command:update /Path:\"%s\" /closeonend:1"
			_execscc(ctx,NULL, ctx->svnpucmd, ctx->lpProjName);
		}
		ctx->dwLastUpdateTime=GetTickCount();
	}
		
	return true;
}


/** Scans work copy and builds in-memory cache */
bool ScanWC(THECONTEXT* ctx,bool force) {
	if(GetTickCount() - ctx->dwLastScanTime > ctx->cacheTtlMs || force){
		log("ScanWC.\n");
		ctx->svni->reset();
		if( ctx->lockStrategy & LOCKSTRATEGY_LOCK ){
			//for lock strategy we call "svn status" and parse response
			if(_execscc(ctx,NULL,"svn stat --xml --non-interactive --trust-server-cert -u -v \"%s\"",ctx->lpProjName)){
				mstring err;
				if(!parseSvnStatus(ctx->lpOutTmp,ctx->svni,&err)){
					_msg(ctx,err);
					return false;
				}
			}
		}
		if( ctx->lockStrategy & LOCKSTRATEGY_PROP ){
			//for prop strategy we use local files scan
			if(__sccupdate(ctx)){
				entries_scan(ctx->lpProjName, &_entries_scanwc_callback, (void*) ctx , ctx->svnwd);
			}else return false;
		}
		ctx->svni->print(logFile);
		ctx->dwLastScanTime=GetTickCount();
	}
	return true;
}


/** 
 * @param createDirs create destination dirs. Normally used for SccAdd.
 */
bool _files2list(THECONTEXT*ctx, LONG nFiles, LPCSTR* lpFileNames, BOOL createDirs){
	bool ret=true;
	FILE*f=fopen(ctx->lpTargetsTmp,"wt");
	if(f){
		for(int j=0;j<nFiles;j++){
			char * fpath=_subst( ctx, lpFileNames[j] );
			if(createDirs){
				for(int i=0; fpath[i]; i++){
					if(fpath[i]=='/' || fpath[i]=='\\'){
						fpath[i]=0;
						if( access(fpath,0) ){
							if(!CreateDirectory(fpath,NULL)){
								log("can't create dir %s\n",fpath);
								ret=false;
							}
							fputs( fpath, f );
							fputs( "\n", f );
						}
						fpath[i]='\\';
					}
				}
			}
			fputs( fpath, f );
			fputs( "\n", f );
		}
		
		fflush(f);
		fclose(f);
		return ret;
	}
	return false;
}

bool _msg2file(THECONTEXT*ctx, char*prefix){
	FILE*f=fopen(ctx->lpMsgTmp,"wb");
	if(f){
		if(prefix && ctx->messagePrefix)fputs( prefix , f );
		fputs( ctx->comment->c_str() , f );
		fflush(f);
		fclose(f);
		return true;
	}
	return false;
}

BOOL _scccommit(THECONTEXT*ctx,SCCCOMMAND icmd){
	char * cmd=NULL;
	switch(icmd){
		case SCC_COMMAND_CHECKOUT:{
			cmd="svn commit --non-interactive --trust-server-cert --targets \"%s\" -m \"scc check out\"";
			break;
		}
		case SCC_COMMAND_UNCHECKOUT:{
			cmd="svn commit --non-interactive --trust-server-cert --targets \"%s\" -m \"scc undo check out\"";
			break;
		}
		case SCC_COMMAND_CHECKIN:{
			_msg2file(ctx,"scc check in : ");
			cmd="svn commit --non-interactive --trust-server-cert --targets \"%s\" --file \"%s\"";
			break;
		}
		case SCC_COMMAND_ADD:{
			_msg2file(ctx,"scc add : ");
			cmd="svn commit --non-interactive --trust-server-cert --targets \"%s\" --file \"%s\"";
			ctx->isLastAddRemove = true;
			break;
		}
		case SCC_COMMAND_REMOVE:{
			_msg2file(ctx,"scc remove : ");
			cmd="svn commit --non-interactive --trust-server-cert --targets \"%s\" --file \"%s\"";
			ctx->isLastAddRemove = true;
			break;
		}
		default : return false;
	}
	if(!_execscc(ctx, NULL, cmd, ctx->lpTargetsTmp, ctx->lpMsgTmp)){
		return false;
	}
	ctx->dwLastCommitTime = GetTickCount();
	return true;
}

BOOL CALLBACK TheEnumWindowsProc(HWND hwnd,LPARAM lParam){
	
	DWORD pid;
	GetWindowThreadProcessId(hwnd,&pid);
	if(pid==GetCurrentProcessId()){
		char c[100]="";
		GetClassName(hwnd, c, sizeof(c));
		if(!strcmp(c,"ConsoleWindowClass")){
			log("console window = %X\n",hwnd);
			consoleHwnd=hwnd;
			ShowWindow(hwnd,SW_HIDE);
			return false;
		}
	}
	return true;
}


void addUniqueListItem(HWND hWnd,int ID,char*c){
	int i=SendDlgItemMessage(hWnd,ID,CB_FINDSTRINGEXACT,-1,(LPARAM)c);
	if(i==CB_ERR){
		SendDlgItemMessage(hWnd,ID,CB_ADDSTRING,0,(LPARAM)c);
	}
}

char*_unquote(char*c){
	int i=strlen(c);
	if(c[0]=='"' && c[i-1]=='"'){c[i-1]=0;c++;}
	return c;
}


BOOL CALLBACK DialogProcComment(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
		case WM_CLOSE:
			EndDialog(hwnd,0);
			break;
		case WM_MANAGEOK:{
				THECONTEXT*ctx;
				ctx=(THECONTEXT*)GetWindowLong(hwnd,GWL_USERDATA);
				BOOL enableOK;
				enableOK=TRUE;
				enableOK &= ( SendDlgItemMessage(hwnd,IDC_EDIT_MSG,WM_GETTEXTLENGTH,0,0) >0 );

				EnableWindow( GetDlgItem(hwnd,IDOK)  , enableOK );
				break;
			}
		case WM_COMMENTHST:{
			//(bool)wParam:  true=store list, false=loda list
			//lParam: if wParam=true, lParam = buffer with current text.
			HKEY rkey;
			int i;
			DWORD type=REG_SZ;
			char buf[PBSCC_MSGLEN+3];
			char key[10];
			DWORD buflen=sizeof(buf);
			if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
				if(wParam){
					//find/replace old value in a list.
					i=(int)SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_FINDSTRINGEXACT,-1,lParam);
					if(i!=CB_ERR)SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_DELETESTRING,i,0);
					SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_INSERTSTRING,1,lParam);
				}
				for(i=1;i<=PBSCC_HSTCNT;i++){
					sprintf(key,"cmt.%02i",i-1);
					if(wParam){
						if(i< SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETCOUNT,0,0) ){
							if( SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETLBTEXTLEN,i,0)<=PBSCC_MSGLEN ){
								SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETLBTEXT,i,(LPARAM) buf);
								RegSetValueEx(rkey,key,0,REG_SZ	,(LPBYTE)buf,(DWORD) strlen(buf)+1);
							}
						}
					}else{
						int err;
						buflen=PBSCC_MSGLEN+2;
						if(i==1)SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_ADDSTRING,0,(LPARAM)"");
						err=RegQueryValueEx(rkey,key,NULL,&type,(LPBYTE)buf,&buflen);
						if(err==ERROR_SUCCESS){
							buf[PBSCC_MSGLEN]=0;//just in case. to prevent longer rows.
							SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_ADDSTRING,0,(LPARAM)buf);
						}else{
							break;
						}
					}
				}
				RegCloseKey(rkey);
			}
			break;
		}
		case WM_SIZE:
			//no resize implemented
			break;
		case WM_COMMAND:
			switch ( LOWORD(wParam) ){
				case IDCANCEL:
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDOK:
					THECONTEXT*ctx;
					ctx=(THECONTEXT*)GetWindowLong(hwnd,GWL_USERDATA);
					ctx->comment->getWindowText(hwnd,IDC_EDIT_MSG);
					//store entered message
					SendMessage(hwnd,WM_COMMENTHST,true, (LPARAM)ctx->comment->c_str());
					//close window
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDC_EDIT_MSG:
					PostMessage(hwnd,WM_MANAGEOK,0,0);
					break;
				case IDC_COMBO_MSG:
					switch( HIWORD(wParam) ){
						//case CBN_SELCHANGE:
						//case CBN_CLOSEUP:
						case CBN_SELENDOK:
							//if( SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_GETCURSEL,0,0) ==-1 )break;
							mstring s=mstring();
							s.getWindowText(hwnd,IDC_COMBO_MSG);
							if(s.len()>0){
								SendDlgItemMessage(hwnd,IDC_EDIT_MSG,WM_SETTEXT,0,(LPARAM) s.c_str());
								PostMessage(hwnd,UM_SETFOCUS,IDC_EDIT_MSG,0);
								SendDlgItemMessage(hwnd,IDC_COMBO_MSG,CB_SETCURSEL,0,(LPARAM) 0);
								PostMessage(hwnd,WM_MANAGEOK,0,0);
							}
							break;
					};
					break;
			}
			return 1;   
		case WM_INITDIALOG:{
			THECONTEXT *ctx;
			LONG i;
			char *sCommand;
			ctx=(THECONTEXT*)lParam ;
			SetWindowLong(hwnd,	GWL_USERDATA,(LONG) lParam);
			SendDlgItemMessage(hwnd,IDC_EDIT_MSG,CB_LIMITTEXT,PBSCC_MSGLEN,0);
			
			
			switch(ctx->eSCCCommand){
				case SCC_COMMAND_CHECKOUT: 
					sCommand="CheckOut";
					break;
				case SCC_COMMAND_CHECKIN:
					sCommand="CheckIn";
					break;
				case SCC_COMMAND_UNCHECKOUT:
					sCommand="UnCheckOut";
					break;
				case SCC_COMMAND_ADD:
					sCommand="Add";
					break;
				case SCC_COMMAND_REMOVE:
					sCommand="Remove";
					break;
				default :
					sCommand="other";
					break;
			}
			
			SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)"command : ");
			SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)sCommand);
			SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)"\r\n");
			for(i=0;i<ctx->nFiles;i++){
				SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)ctx->lpFileNames[i]);
				SendDlgItemMessage(hwnd,IDC_PATH,EM_REPLACESEL,0,(LPARAM)"\r\n");
			}

			PostMessage(hwnd,UM_SETFOCUS,IDC_EDIT_MSG,0);
			//restore message history
			SendMessage(hwnd,WM_COMMENTHST,false,0);
			//manage OK button (disable in this case)
			PostMessage(hwnd,WM_MANAGEOK,0,0);
			return 1;
		}
		case UM_SETFOCUS:
			SetFocus( GetDlgItem(hwnd,wParam) );
			break;
	}
	return 0;
}



BOOL _copyfile(THECONTEXT *ctx,const char*src,char*dst){
	log("\tcopy \"%s\" \"%s\".\n",src,dst);
	SetFileAttributes(src,FILE_ATTRIBUTE_NORMAL);
	SetFileAttributes(dst,FILE_ATTRIBUTE_NORMAL);
	BOOL b=false;
	if(ctx->exportEncode==EENCODE_NONE){
		b=CopyFile(src,dst,false);
	}else{
		b=CopyFileUTF8(src,dst);
		//try usual copy
		if(!b)b=CopyFile(src,dst,false);
	}
	if(!b){
		char *buf=new char[strlen(src)+strlen(dst)+100];
		sprintf(buf,"can't copy \"%s\" to \"%s\"",src,dst);
		log("%s.\n",buf);
		_msg(ctx,buf);
		delete []buf;
	}
	return b;
}

BOOL needComment(THECONTEXT*ctx){
	//a little bit stupid check, but...
	//if all files are *.pbg then no comment needed
	for(int i=0;i<ctx->nFiles;i++){
		long len=strlen(ctx->lpFileNames[i]);
		if( len>4 && !stricmp(".pbg",ctx->lpFileNames[i]+len-4) ){
			//this is a last file and all files are pbg
			if(i==ctx->nFiles-1){
				log("needComment: only pbg files. no comment needed.\n");
				return false;
			}
		}else break;
	}
	if ((GetTickCount() - ctx->dwLastCommitTime) > DELAYFORNEWCOMMENT){
		log("needComment: true.\n");
		return true;
	}
	log("needComment: false.\n");
	return false;
}

bool rememberTarget(THECONTEXT*ctx,LONG nFiles,LPCSTR* lpFileNames){
	for(LONG i=0;i<nFiles;i++){
		long len=strlen(lpFileNames[i]);
		if( len>4 && !stricmp(".pbt",lpFileNames[i]+len-4) && len<MAXFULLPATH ){
			strcpy(ctx->PBTarget,lpFileNames[i]);
			//log("target: \"%s\"\n",ctx->PBTarget);
		}
	}
	return true;
}


//file just for information 
bool _getcomment(THECONTEXT*ctx,HWND hWnd,LONG nFiles, LPCSTR* lpFileNames,SCCCOMMAND icmd){
	int dlgID=IDD_DIALOG_COMMENT2;
	ctx->nFiles=nFiles;
	ctx->lpFileNames=lpFileNames;
	ctx->eSCCCommand=icmd;
	
	if( !needComment(ctx) ) return true;
	ctx->comment->set(NULL);
	if( DialogBoxParam(hInstance,MAKEINTRESOURCE(dlgID),hWnd,&DialogProcComment,(LPARAM)ctx)==0 ){
		log("_getcomment: %s\n",ctx->comment->c_str());
		if(ctx->comment->len()>0)return true;
	}
	return false;
}


extern "C"{
	
void PbSccVersion(){
	FILE*f=fopen("pbscc.ver","wt");
	fprintf(f,"version=%s\n",PROJECT_VER);
	fprintf(f,"date=%s\n",PROJECT_DATE);
	fflush(f);
	fclose(f);
}

	
SCCEXTERNC SCCRTN EXTFUN SccInitialize(LPVOID * ppContext, HWND hWnd, LPCSTR lpCallerName,LPSTR lpSccName, LPLONG lpSccCaps, LPSTR lpAuxPathLabel, LPLONG pnCheckoutCommentLen, LPLONG pnCommentLen){
	log("SccInitialize:\n");
	
	THECONTEXT * ctx=new THECONTEXT;
	memset( ctx, 0, sizeof(THECONTEXT) );
	strcpy(ctx->lpCallerName,lpCallerName);
	strcpy(lpSccName,gpSccName);
	lpSccCaps[0]=0x200828D|SCC_CAP_PROPERTIES;
	lpAuxPathLabel[0]=0;
	pnCheckoutCommentLen[0]=PBSCC_MSGLEN+1;
	pnCommentLen[0]=PBSCC_MSGLEN+1;
	ppContext[0]=ctx;
	
	
	if(!consoleHwnd){
		//set window topmost flag to prevent console visible
		SetWindowPos(hWnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOREDRAW|SWP_NOSENDCHANGING|SWP_NOSIZE);
		AllocConsole();
		EnumWindows(&TheEnumWindowsProc,0);
		//remove window topmost flag
		SetWindowPos(hWnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOREDRAW|SWP_NOSENDCHANGING|SWP_NOSIZE);
	}
	
	HKEY rkey;
	char buf[1024];
	DWORD type=REG_SZ;
	DWORD buflen;
	
	if( RegOpenKey(PBSCC_REGKEY,PBSCC_REGPATH,&rkey)==ERROR_SUCCESS){
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"cache.ttl.seconds",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			ctx->cacheTtlMs=atol(buf);
			if(ctx->cacheTtlMs<20)	ctx->cacheTtlMs=20;
			//we need it in milliseconds
			ctx->cacheTtlMs=ctx->cacheTtlMs*1000;
		}
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"svn.work",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			strncpy(ctx->svnwd,buf,SCC_USER_LEN);
			ctx->svnwd[SCC_USER_LEN-1]=0;
		}else{
			strcpy(ctx->svnwd,".svn"); //get default value
		}
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"svn.postupdatecmd",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			strncpy(ctx->svnpucmd,buf,buflen);
		}else{
			strcpy(ctx->svnpucmd,""); //get default value
		}
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"svn.diffdelay",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			ctx->svndiff_delay = atol( buf );
		}
		else{
			ctx->svndiff_delay = 500;
		}
		
		buflen=sizeof(buf);
		if(RegQueryValueEx(rkey,"svn.diffmethod",NULL,&type,(LPBYTE)buf,&buflen)==ERROR_SUCCESS){
			ctx->svndiff_method = atol( buf );
		}
		else{
			ctx->svndiff_method = 1;
		}
		
		RegCloseKey(rkey);
	}
	
	DWORD len=GetTempPath(MAXFULLPATH,ctx->lpTargetsTmp);
	strcpy(ctx->lpOutTmp,ctx->lpTargetsTmp);
	strcpy(ctx->lpErrTmp,ctx->lpTargetsTmp);
	strcpy(ctx->lpMsgTmp,ctx->lpTargetsTmp);
	
	GetTempFileName(ctx->lpTargetsTmp,"pbscc",0,ctx->lpTargetsTmp );
	GetTempFileName(ctx->lpOutTmp    ,"pbscc",0,ctx->lpOutTmp     );
	GetTempFileName(ctx->lpErrTmp    ,"pbscc",0,ctx->lpErrTmp     );
	GetTempFileName(ctx->lpMsgTmp    ,"pbscc",0,ctx->lpMsgTmp     );
	
	log("\t cache.ttl.milli: %i\n",ctx->cacheTtlMs);
	log("\t svn work dir   : %s\n",ctx->svnwd);
	log("\t out pipe       : %s\n",ctx->lpOutTmp);
	log("\t err pipe       : %s\n",ctx->lpErrTmp);
	log("\t msg pipe       : %s\n",ctx->lpMsgTmp);
	
	ctx->comment=new mstring();
	ctx->pipeErr=new mstring();
	ctx->svni=new svninfo();
	
	ctx->parent=hWnd;
	
	//the last command/ just to test
	_execscc(ctx, NULL,"%s","set");
	
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccUninitialize(LPVOID pContext){
	log("SccUninitialize:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	
	DeleteFile(ctx->lpTargetsTmp); //delete temp file
	DeleteFile(ctx->lpOutTmp); //delete temp file
	DeleteFile(ctx->lpErrTmp); //delete temp file
	DeleteFile(ctx->lpMsgTmp); //delete temp file
	
	delete ctx->comment;
	delete ctx->pipeErr;
	delete ctx->svni;


	delete pContext;
	if(consoleHwnd){
		FreeConsole();
		consoleHwnd=NULL;
	}
	return SCC_OK;
}


SCCEXTERNC SCCRTN EXTFUN SccGetProjPath(LPVOID pContext, HWND hWnd, LPSTR lpUser,LPSTR lpProjName, LPSTR lpLocalPath,LPSTR lpAuxProjPath,BOOL bAllowChangePath,LPBOOL pbNew){
	log("SccGetProjPath:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	BROWSEINFO bi;
	LPITEMIDLIST id;
	SCCRTN ret=SCC_E_OPNOTSUPPORTED;
	LPMALLOC lpMalloc;
	
	
	memset(&bi,0,sizeof(bi));
	bi.hwndOwner=hWnd;
	bi.pszDisplayName=lpProjName;
	bi.lpszTitle="Select the folder under source control";
	repeat:
	id=SHBrowseForFolder( &bi );
	if(id){
		SHGetPathFromIDList(id, lpProjName);
		if(SHGetMalloc(&lpMalloc)==NOERROR)lpMalloc->Free(id);
		if(!_execscc(ctx, NULL,"svn info -r BASE --non-interactive --trust-server-cert \"%s\"",lpProjName)){
			char buf[2048];
			_snprintf(buf,sizeof(buf)-1,"The selected folder is not under source control or SVN repository is not available.\n\n"  
				"The command line\n"
				"\tsvn.exe info -r BASE --non-interactive --trust-server-cert \"%s\"\n"
				"\tfailed.\n\n"
				"Check that svn.exe installed on your computer.\n"
				"The directory where svn.exe is located must be in the PATH environment variable.\n"
				"Start your command shell and check that the command specified above returns no errors.\n"
				,lpProjName);
			strcpy(buf+sizeof(buf)-6,"...");
			MessageBox(hWnd,buf, gpSccName, MB_OK|MB_ICONERROR);
			goto repeat;
		}
		
		DWORD user_len=SCC_USER_LEN;
		GetUserName(lpUser,&user_len);
		lpAuxProjPath[0]=0;
		pbNew[0]=false;
		ret=SCC_OK;
	}
	return ret;
}



SCCEXTERNC SCCRTN EXTFUN SccOpenProject(LPVOID pContext,HWND hWnd, LPSTR lpUser,LPSTR lpProjName,LPCSTR lpLocalProjPath,LPSTR lpAuxProjPath,LPCSTR lpComment,LPTEXTOUTPROC lpTextOutProc,LONG dwFlags){
	log("SccOpenProject:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	mstring buf=mstring();
	mstring s=mstring("");
	strcpy(ctx->lpProjName,lpProjName);
	strcpy(ctx->lpProjPath,lpLocalProjPath);
	strcpy(ctx->lpUser,lpUser);
	ctx->cbProjName=strlen(ctx->lpProjName);
	ctx->cbProjPath=strlen(ctx->lpProjPath);
	ctx->lpOutProc=lpTextOutProc;
	
	_msg(ctx,buf.set(NULL)->sprintf("version %s built on %s",PROJECT_VER,PROJECT_DATE)->c_str());
	_msg(ctx,buf.set(NULL)->sprintf("svn work dir: %s",ctx->svnwd)->c_str() );
	
	if(!__sccupdate(ctx))return SCC_E_INITIALIZEFAILED;
	
	{
		//path to the scc.ini file
		buf.set(ctx->lpProjName)->addPath("scc.ini");
		//if file does not exist, suggest to create it
		if( access( buf.c_str(), 0 /*R_OK*/) ) {
			mstring msg=mstring("Attention!\n");
			msg.sprintf("Your svn project \"%s\"\ndoes not have definition of the object locking strategy.\n",ctx->lpProjName);
			msg.sprintf("This information will be stored here:\n \"%s\"\n\n",buf.c_str());
			
			msg.append("You should choose:\n");
			msg.append("\tYes \tto use \"svn lock\"\n");
			msg.append("\tNo  \tto use \"lockby\" property (old behavior)\n");
			msg.append("\tCancel\tto answer this question later\n");
			
			int choise = MessageBox(hWnd, msg.c_str(), gpSccName, MB_YESNOCANCEL|MB_ICONINFORMATION );
			
			if(choise==IDYES || choise==IDNO){
				msg.set   ("[config]\n")->
					append("; lock strategy: \n")->
					append(";   \"lock\"  using svn lock (default from version 2.x)\n")->
					append(";   \"prop\"  using svn properties (old behavior)\n")->
					append("lock.strategy=");
				
				if(choise==IDYES)msg.append("lock\n");
				else msg.append("prop\n");
				//create scc.ini file
				FILE*f=fopen(buf.c_str(),"wt");
				if(f){
					fputs(msg.c_str(),f);
					fflush(f);
					fclose(f);
					//add file into svn
					if(_execscc(ctx, NULL,"svn add --non-interactive --trust-server-cert \"%s\"",buf.c_str())  ){
						if(!_execscc(ctx, NULL,"svn commit --non-interactive --trust-server-cert -m \"project init\" \"%s\"",buf.c_str()))
							return SCC_E_INITIALIZEFAILED;
					}else{
						_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert \"%s\"",buf.c_str());
						return SCC_E_INITIALIZEFAILED;
					}
				}
			}
		}
		
		log("read scc.ini\n");
		//get message prefix
		s.getIniString("config","message.prefix","", buf.c_str() );
		if(!strcmp(s.c_str(),"true"))ctx->messagePrefix=true;
		else ctx->messagePrefix=false;
		

		
		//get lock strategy
		s.getIniString("config","lock.strategy","", buf.c_str() );
		if(!strcmp(s.c_str(),"prop"))ctx->lockStrategy=LOCKSTRATEGY_PROP;
		else ctx->lockStrategy=LOCKSTRATEGY_LOCK;
		
		s.set("lock strategy : ");
		if(ctx->lockStrategy&LOCKSTRATEGY_LOCK)s.append("lock");
		if(ctx->lockStrategy&LOCKSTRATEGY_PROP)s.append("prop");
		_msg(ctx,s.c_str() );
		log("\t%s\n",s.c_str());
		
		//get recode flag
		s.getIniString("config","export.encoding","", buf.c_str() );
		if(!stricmp(s.c_str(),"utf-8"))ctx->exportEncode=EENCODE_UTF8;
		else ctx->exportEncode=EENCODE_NONE;
		
		s.set("export encoding : ");
		if(ctx->exportEncode==EENCODE_UTF8)s.append("utf-8");
		if(ctx->exportEncode==EENCODE_NONE)s.append("none");
		_msg(ctx,s.c_str() );
		log("\t%s\n",s.c_str());
		
	}
	ScanWC(ctx,false);
	
	if(!PBGetVersion(ctx->PBVersion))ctx->PBVersion[0]=0;//get pb version
	
	return SCC_OK;
}


SCCEXTERNC SCCRTN EXTFUN SccGetCommandOptions(LPVOID pContext, HWND hWnd, enum SCCCOMMAND nCommand,LPCMDOPTS * ppvOptions){
	log("SccGetCommandOptions:\n");
	if(nCommand==SCC_COMMAND_OPTIONS)return SCC_OK;//SCC_I_ADV_SUPPORT; //no advanced button
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccCloseProject(LPVOID pContext){
	log("SccCloseProject:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccGet(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LONG dwFlags,LPCMDOPTS pvOptions){
	log("SccGet:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	if(GetTickCount() - ctx->dwLastGetTime > DELAYFORNEWCOMMENT) {
		if(!__sccupdate(ctx))return SCC_E_ACCESSFAILURE;
		ScanWC(ctx,true);
	}
	for(int i=0;i<nFiles;i++){
		log("\t%s\n",lpFileNames[i]);
		if( !_copyfile(ctx,_subst(ctx,(char*)lpFileNames[i]),(char*)lpFileNames[i]) )return SCC_E_NONSPECIFICERROR;
	}
	ctx->dwLastGetTime=GetTickCount();
	return SCC_OK;
}

//common function for SccQueryInfoEx and SccQueryInfo 
//the difference only in cbFunc and cbParm parameters
SCCRTN _SccQueryInfo(LPVOID pContext, LONG nFiles, LPCSTR* lpFileNames,LPLONG lpStatus,INFOEXCALLBACK cbFunc,LPVOID cbParm){
	INFOEXCALLBACKPARM cbp;
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	const char* errFile=NULL;
	int svniErr;
	//ctx->lpComment[0]=0;//guess this is deprecated line
	DWORD t=GetTickCount();
	if(cbFunc){
		memset(&cbp,0,sizeof(cbp));
		cbp.cb=sizeof(cbp);
		cbp.status=1;
	}
	rememberTarget(ctx, nFiles, lpFileNames);
	if(!ScanWC(ctx,false))return SCC_E_ACCESSFAILURE;

	for(int i=0;i<nFiles;i++){
		lpStatus[i]=SCC_STATUS_NOTCONTROLLED;
		SVNINFOITEM * svni = ctx->svni->get(ctx->lpProjPath,lpFileNames[i], &svniErr);
		log("\tsvni->get( \"%s\" , \"%s\" )=%s\n",ctx->lpProjPath,lpFileNames[i],(svni?svni->owner:"null"));
		if( svni !=NULL ){
			lpStatus[i]=SCC_STATUS_CONTROLLED;
			if(svni->owner[0]){
				if(svni->isOwner)lpStatus[i]=SCC_STATUS_OUTBYUSER|SCC_STATUS_CONTROLLED;
				else lpStatus[i]=SCC_STATUS_OUTOTHER|SCC_STATUS_CONTROLLED;
			}
			if(cbFunc){
				cbp.object=(char*)lpFileNames[i];
				cbp.version=svni->rev;
				cbFunc(cbParm,&cbp);
			}
		}else{
			if(svniErr){
				if(!errFile)errFile=lpFileNames[i];
				log("\tWARN: Path is out of root\n");
			}
			if(cbFunc){
				cbp.object=(char*)lpFileNames[i];
				cbp.version="";
				cbFunc(cbParm,&cbp);
			}
		}
		//log("\tstat=%04X ver=\"%s\" user=\"%s\" \"%s\"\n",lpStatus[i],ver,user,lpFileNames[i]);
	}
	if(errFile){
		mstring s=mstring("WARN: Path is out of root directory: ");
		s.append(errFile);
		_msg(ctx, s.c_str());
		_msg(ctx,"Probably wrong scc parameter \"Local Root Directory\". It must include all subdirectories of your project." );
	}
	log("_SccQueryInfo: ms=%i\n",GetTickCount()-t);
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccQueryInfoEx(LPVOID pContext, LONG nFiles, LPCSTR* lpFileNames,LPLONG lpStatus,INFOEXCALLBACK cbFunc,LPVOID cbParm){
	log("SccQueryInfoEx:\n");
	return _SccQueryInfo(pContext, nFiles, lpFileNames, lpStatus, cbFunc, cbParm);
}

SCCEXTERNC SCCRTN EXTFUN SccQueryInfo(LPVOID pContext, LONG nFiles, LPCSTR* lpFileNames, LPLONG lpStatus){
	log("SccQueryInfo:\n");
	return _SccQueryInfo(pContext, nFiles, lpFileNames, lpStatus, NULL, NULL);
}

SCCEXTERNC SCCRTN EXTFUN SccCheckout(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LPCSTR lpComment, LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	int i;
	log("SccCheckout:\n");
	//do svn operations
	if (!_files2list(ctx, nFiles, lpFileNames,false ))return SCC_E_ACCESSFAILURE;
	
	if( ctx->lockStrategy&LOCKSTRATEGY_PROP ) {
		if(!ScanWC(ctx,true))return SCC_E_ACCESSFAILURE;
		//do preliminary check
		for(i=0;i<nFiles;i++){
			SVNINFOITEM * svni = ctx->svni->get(ctx->lpProjPath,lpFileNames[i],NULL);
			log("\tsvni->get( \"%s\" , \"%s\" )=%s\n",ctx->lpProjPath,lpFileNames[i],(svni?svni->owner:"null"));
			if( svni!=NULL ){
				if(svni->owner[0])return SCC_E_ALREADYCHECKEDOUT;
			}else return SCC_E_FILENOTCONTROLLED;
		}
		if(!_execscc(ctx, NULL,"svn propset --non-interactive --trust-server-cert lockby \"%s\" --targets \"%s\"",ctx->lpUser,ctx->lpTargetsTmp)  )return SCC_E_NONSPECIFICERROR;
		if(!_scccommit(ctx,SCC_COMMAND_CHECKOUT )){
			_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp);
			return SCC_E_ACCESSFAILURE;
		}
	}
	if( ctx->lockStrategy&LOCKSTRATEGY_LOCK ) {
		if(!__sccupdate(ctx))return SCC_E_ACCESSFAILURE;
		if(!_execscc(ctx, NULL,"svn lock --non-interactive --trust-server-cert --targets \"%s\" -m CheckOut",ctx->lpTargetsTmp)){
			return SCC_E_ACCESSFAILURE;
		}
	}
	
	//finish operations
	for( i=0;i<nFiles;i++){
		_copyfile(ctx,_subst(ctx,(char*)lpFileNames[i]),(char*)lpFileNames[i]);
		//add link into todo list
		ToDoSetLine("n", ctx->PBTarget,  (char*)lpFileNames[i], ctx->PBVersion);
	}
	//reset scan time to force scanning
	ctx->dwLastScanTime=0;
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccUncheckout(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LONG dwFlags,LPCMDOPTS pvOptions){
	int i;
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	log("SccUncheckout:\n");
	
	if (!_files2list(ctx, nFiles, lpFileNames, false ))return SCC_E_ACCESSFAILURE;
	
	if( ctx->lockStrategy&LOCKSTRATEGY_LOCK ) {
		if(!_execscc(ctx, NULL,"svn unlock --non-interactive --trust-server-cert --force --targets \"%s\" ",ctx->lpTargetsTmp))goto error;
	}
	
	if( ctx->lockStrategy&LOCKSTRATEGY_PROP ) {
		if(!ScanWC(ctx,true))return SCC_E_ACCESSFAILURE;
		//do preliminary check
		for(i=0;i<nFiles;i++){
			SVNINFOITEM * svni;
			if( (svni = ctx->svni->get(ctx->lpProjPath,lpFileNames[i],NULL))!=NULL ){
				if( !svni->isOwner ) return SCC_E_NOTCHECKEDOUT;
			}else return SCC_E_FILENOTCONTROLLED;
		}
		for(i=0;i<nFiles;i++){
			if(!_execscc(ctx, NULL,"svn propdel --non-interactive --trust-server-cert lockby \"%s\"",_subst(ctx,lpFileNames[i]))  )
				goto error;
		}
		if(!_scccommit(ctx,SCC_COMMAND_UNCHECKOUT ))goto error;
	}
	
	for(i=0;i<nFiles;i++){
		_copyfile(ctx,_subst(ctx,(char*)lpFileNames[i]),(char*)lpFileNames[i]);
		//cross the link in the todo list
		ToDoSetLine("y", ctx->PBTarget,  (char*)lpFileNames[i], ctx->PBVersion);
	}
	//reset scan time to force scanning
	ctx->dwLastScanTime=0;
	return SCC_OK;
	error:
	_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp);
	//reset scan time to force scanning
	ctx->dwLastScanTime=0;
	return SCC_E_ACCESSFAILURE;
}

SCCEXTERNC SCCRTN EXTFUN SccCheckin(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LPCSTR lpComment, LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	int i;
	log("SccCheckin: \n");
	
	if (!_files2list(ctx, nFiles, lpFileNames,false ))return SCC_E_ACCESSFAILURE;
	if( !_getcomment( ctx, hWnd,nFiles,lpFileNames,SCC_COMMAND_CHECKIN) )return SCC_I_OPERATIONCANCELED;
	
	if( ctx->lockStrategy&LOCKSTRATEGY_LOCK ) {
		//nothing special for lock
	}
	
	if( ctx->lockStrategy&LOCKSTRATEGY_PROP ) {
		if(!ScanWC(ctx,true))return SCC_E_ACCESSFAILURE;
		
		for(i=0;i<nFiles;i++){
			SVNINFOITEM * svni;
			if( (svni = ctx->svni->get(ctx->lpProjPath,lpFileNames[i],NULL))!=NULL ){
				if( !svni->isOwner ) return SCC_E_NOTCHECKEDOUT;
			}else return SCC_E_FILENOTCONTROLLED;
			if(!_execscc(ctx, NULL,"svn propdel --non-interactive --trust-server-cert lockby \"%s\"",_subst(ctx,lpFileNames[i]))  )
				goto error;
		}
	}
	for(i=0;i<nFiles;i++){
		if( !_copyfile(ctx,lpFileNames[i],_subst(ctx, (char*)lpFileNames[i])) )goto error;
	}
	//locks automatically removed on commit.
	if(!_scccommit(ctx,SCC_COMMAND_CHECKIN ))goto error;
	
	for(i=0;i<nFiles;i++){
		//cross the link in the todo list
		ToDoSetLine("y", ctx->PBTarget,  (char*)lpFileNames[i], ctx->PBVersion);
	}
	//reset scan time to force scanning
	ctx->dwLastScanTime=0;
	return SCC_OK;
	error:
	_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp);
	//reset scan time to force scanning
	ctx->dwLastScanTime=0;
	return SCC_E_ACCESSFAILURE;
}

SCCEXTERNC SCCRTN EXTFUN SccAdd(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LPCSTR lpComment, LONG * pdwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	int i;
	log("SccAdd:\n");
	//reset scan time to force scanning
	ctx->dwLastScanTime=0;
	
	if (!_files2list(ctx, nFiles, lpFileNames, true ))return SCC_E_ACCESSFAILURE;
	if( !_getcomment( ctx, hWnd,nFiles,lpFileNames,SCC_COMMAND_ADD) )return SCC_I_OPERATIONCANCELED;
	
	if(!__sccupdate(ctx))return SCC_E_ACCESSFAILURE;
	
	for(i=0;i<nFiles;i++){
		if( !_copyfile(ctx,lpFileNames[i],_subst(ctx, (char*)lpFileNames[i])) )return SCC_E_FILENOTEXIST;
	}
	if(!_execscc(ctx, NULL,"svn add --non-interactive --force --non-recursive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp)  )goto error;
	if(!_scccommit(ctx,SCC_COMMAND_ADD ))goto error;
	return SCC_OK;
	error:
	_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp);
	for(i=0;i<nFiles;i++){
		DeleteFile(_subst(ctx, (char*)lpFileNames[i]));
	}
	return SCC_E_ACCESSFAILURE;
	
}

SCCEXTERNC SCCRTN EXTFUN SccRemove(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames,LPCSTR lpComment,LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	log("SccRemove:\n");
	
	if( !_getcomment( ctx, hWnd,nFiles,lpFileNames,SCC_COMMAND_REMOVE) )return SCC_I_OPERATIONCANCELED;
	if (!_files2list(ctx, nFiles, lpFileNames,false ))return SCC_E_ACCESSFAILURE;

	for(int i=0;i<nFiles;i++){
		SetFileAttributes(_subst(ctx, (char*)lpFileNames[i]),FILE_ATTRIBUTE_NORMAL);
	}
	
	if(!_execscc(ctx, NULL,"svn del --non-interactive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp)  )goto error;
	if(!_scccommit(ctx,SCC_COMMAND_REMOVE ))goto error;
	return SCC_OK;
	error:
	_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert --targets \"%s\"",ctx->lpTargetsTmp);
	return SCC_E_ACCESSFAILURE;
}

SCCEXTERNC SCCRTN EXTFUN SccDiff(LPVOID pContext, HWND hWnd, LPCSTR lpFileName, LONG dwFlags,LPCMDOPTS pvOptions){
	log("SccDiff: %s, %X :\n",lpFileName,dwFlags);
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	
	if( access( _subst(ctx,lpFileName), 0 /*R_OK*/ ) )return SCC_E_FILENOTCONTROLLED;
	
	if(dwFlags&SCC_DIFF_QUICK_DIFF){
		log("\tdo quick diff\n");
		//do quick diff
		if( filecmp( lpFileName, _subst(ctx,lpFileName) )  ){
			return SCC_OK;
		}
		return SCC_I_FILEDIFFERS;
	}else{
		log("\tdo visual diff\n");
		mstring buf;
		SVNINFOITEM * svni;
		if( ( svni = ctx->svni->get(ctx->lpProjPath,lpFileName,NULL))!=NULL ) {
			if(svni->isOwner){
				if(ctx->svndiff_method==0){
					_copyfile(ctx,lpFileName, _subst(ctx,lpFileName) );
					buf.sprintf("TortoiseProc.exe /command:diff /path:\"%s\"", _subst(ctx,lpFileName) );
				}
				else{
					char tmppath[1024];
					GetTempPath(1023, tmppath);
					char* rootFilename = _rootfilename(ctx,lpFileName);
					char tmpFileName[1024];
					GetTempFileName(tmppath ,"pbscc",0,tmpFileName);
					//~ char* tmpFileName = _dupsrc(ctx, tmppath, lpFileName);
					_copyfile(ctx,lpFileName, tmpFileName );
					buf.sprintf("pbsccproxydiff.cmd \"%s\" \"%s\" \"%s\"", _subst(ctx,lpFileName), tmpFileName, rootFilename);
				}
				WinExec(buf.c_str(), SW_SHOW);
				if(ctx->svndiff_delay) Sleep( ctx->svndiff_delay );
				if(ctx->svndiff_method==0){
					_execscc(ctx, NULL,"svn revert --non-interactive --trust-server-cert \"%s\"", _subst(ctx,lpFileName) );
				}
				return SCC_OK;
			}
		}
		MessageBox(hWnd,"No differences encountered.",gpSccName,MB_ICONINFORMATION);
	}
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccHistory(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames, LONG dwFlags,LPCMDOPTS pvOptions){
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	mstring buf;

	if (nFiles != 1) return SCC_E_NONSPECIFICERROR;
	buf.sprintf("TortoiseProc.exe /command:log /path:\"%s\"", _subst(ctx, (char*)lpFileNames[0]));
	WinExec(buf.c_str(), SW_SHOW);
	
	return SCC_OK;
}


SCCEXTERNC LONG EXTFUN SccGetVersion(){
	log("SccGetVersion %i\n",SCC_VER_NUM);
	return SCC_VER_NUM;
}






//---------------------------------------------------------------------------------------------


SCCEXTERNC SCCRTN EXTFUN SccRename(LPVOID pContext, HWND hWnd, LPCSTR lpFileName,LPCSTR lpNewName){
	log("SccRename: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccProperties(LPVOID pContext, HWND hWnd, LPCSTR lpFileName){
	log("SccProperties:\n");
	THECONTEXT *ctx=(THECONTEXT *)pContext;
	SVNINFOITEM * svni = ctx->svni->get(ctx->lpProjPath,lpFileName, NULL);
	mstring s=mstring();
	if(svni){
		s.sprintf("%s\nrevision: %s\nowner: %s",svni->path,svni->rev,svni->owner);
	}else{
		s.set("not controlled.");
	}
	_msg(ctx,s);
	return SCC_OK;
}

SCCEXTERNC SCCRTN EXTFUN SccPopulateList(LPVOID pContext, enum SCCCOMMAND nCommand, LONG nFiles, LPCSTR* lpFileNames, POPLISTFUNC pfnPopulate, LPVOID pvCallerData,LPLONG lpStatus, LONG dwFlags){
	log("SccPopulateList: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccGetEvents(LPVOID pContext, LPSTR lpFileName,LPLONG lpStatus,LPLONG pnEventsRemaining){
	log("SccGetEvents: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccRunScc(LPVOID pContext, HWND hWnd, LONG nFiles, LPCSTR* lpFileNames){
	log("SccRunScc: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccAddFromScc(LPVOID pContext, HWND hWnd, LPLONG pnFiles,LPCSTR** lplpFileNames){
	log("SccAddFromScc: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

SCCEXTERNC SCCRTN EXTFUN SccSetOption(LPVOID pContext,LONG nOption,LONG dwVal){
	log("SccSetOption: not supported\n");
	return SCC_E_OPNOTSUPPORTED;
}

           
} //extern "C"
