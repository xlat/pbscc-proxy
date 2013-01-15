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
#include <shlobj.h>
#include <io.h>
#include "mstring.h"
#include "conproc.h"


//opens process defined by "cmd" parameter
//and returns FILE* reference to stdout and stderror of this process

BOOL _POPEN(THECONTEXT*ctx, char*cmd, FILE**_stdout,FILE**_stderr){
	mstring _cmd=mstring();
	char lc_messages[101]="";
	char *plc_messages=lc_messages;
	
	_cmd.sprintf("%s >\"%s\" 2>\"%s\"",cmd,ctx->lpOutTmp,ctx->lpErrTmp);
	
	
	if(GetEnvironmentVariable("LC_MESSAGES",lc_messages,sizeof(lc_messages))==0)plc_messages=NULL;
	SetEnvironmentVariable("LC_MESSAGES","en_US");
	if(system(_cmd.c_str())==-1){
		log("error calling system command (%i)\n",errno);
		SetEnvironmentVariable("LC_MESSAGES",plc_messages);
		return false;
	}
	SetEnvironmentVariable("LC_MESSAGES",plc_messages);
	
	_stdout[0] = fopen( ctx->lpOutTmp, "rt" );
	if(!_stdout[0])log("error: %s\n",strerror( errno ));
	_stderr[0] = fopen( ctx->lpErrTmp, "rt" );
	if(!_stderr[0])log("error: %s\n",strerror( errno ));
	
	if ( _stdout[0] && _stderr[0] ) return true;
	
	if ( _stdout[0] ) fclose(_stdout[0]);
	if ( _stderr[0] ) fclose(_stderr[0]);
	_stdout[0]=NULL;
	_stderr[0]=NULL;
	
	return false;
}

//closes FILE* structures opened by _POPEN

void _PCLOSE(FILE*_stdout, FILE*_stderr ){
	if(_stdout)fclose(_stdout);
	if(_stderr)fclose(_stderr);
	
	//DeleteFile(ctx->lpOutTmp);
	//DeleteFile(ctx->lpErrTmp);
}


//returns true if error pipe is empty.
BOOL _execscc(THECONTEXT*ctx,mstring * _pipeOut, char * cmd,char * parm,char * parm2){
	int repeat_cnt=0;
	FILE *fout=NULL;
	FILE *ferr=NULL;
	mstring pipeOutLocal;
	mstring * pipeOut=_pipeOut;
	mstring buf;
	buf.sprintf(cmd,parm,parm2);
	//this will copy stdout into log file
	if( logEnabled() && pipeOut==NULL){
		pipeOut=&pipeOutLocal;
	}
	
	repeat:
	log("exec: %s\n",buf.c_str());
	if(pipeOut)pipeOut->set(NULL);
	ctx->pipeErr->set(NULL);
	
	if( _POPEN( ctx, buf.c_str(), &fout, &ferr )  ) {
    	char fbuf[1024];
    	fbuf[sizeof(fbuf)-1]=0;

    	//read out
    	if(pipeOut){
			while(fgets(fbuf,sizeof(fbuf)-1,fout)){
				pipeOut->append(fbuf);
			}
			pipeOut->rtrim();
			if(pipeOut->len()>0)log("stdout: %s\n",pipeOut->c_str());
    	}

		//read error
   		while(fgets(fbuf,sizeof(fbuf)-1,ferr)){
   			ctx->pipeErr->append(fbuf);
   		}
   		ctx->pipeErr->rtrim();
		if(ctx->pipeErr->len()>0)log("stderr: %s\n",ctx->pipeErr->c_str());
		
		_PCLOSE( fout, ferr );
		
		if( ctx->pipeErr->len()==0 ) return true;
		
		_msg(ctx,ctx->pipeErr->c_str());
		
	    CRegexp regexp("^svn\\s+\\w+\\b");
	    MatchResult mres = regexp.Match(buf.c_str());

		if( ( ctx->pipeErr->match("password",IGNORECASE) || 
			  ctx->pipeErr->match("authentication",IGNORECASE) ||
			  ctx->pipeErr->match("authorization",IGNORECASE) ) &&
			( mres.IsMatched() ) ) 
		{
			//authorization failed?
			//let's request the credentials
			if( !_loginscc(ctx) ) return false;
			mstring buf2=mstring();
			buf2.sprintf(cmd,parm,parm2);
			buf.set(NULL);
			buf.sprintf("%.*s --username \"%s\" --password \"%s\" %s",
				mres.GetEnd(),buf2.c_str(), ctx->uid, ctx->pwd, buf2.c_str()+mres.GetEnd() );
			repeat_cnt++;
			if(repeat_cnt>4)return false;
			goto repeat;
			
		}
		
		
	}else{
		_msg(ctx,"Can't open pipe to the process:");
		_msg(ctx,buf.c_str());
	}
	return false;
}
