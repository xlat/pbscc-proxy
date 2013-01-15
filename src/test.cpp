/* This is simple demonstration of how to use expat. This program
   reads an XML document from standard input and writes a line with
   the name of each element to standard output indenting child
   elements by one tab stop more than their parent element.
   It must be used with Expat compiled for UTF-8 output.
*/

#include <windows.h>
#include <stdio.h>
//#include "expat.h"
#include "pbscc.h"
#include "mstring.h"
#include "entries.h"
#include "svninfo.h"
#include "filecpy.h"

#define BUF_SIZE	512
#define XML_FMT_INT_MOD "l"

extern FILE	*logFile;

long timer(long t,char * msg){
	long i=GetTickCount();
	if(t==0)t=i;
	printf("*** %s timer = %i [%i] **\n",msg,i-t,t);
	return i;
}

long outproc (LPCSTR msg, DWORD len) {
	printf("*** outproc: ");
	for(DWORD i=0; i<len; i++){
		putchar(msg[i]);
	}
	putchar('\n');
	return 0;
}


int main(int argc, char *argv[]) {
	long t=0;
	int i;
	
	//printf("copy=%i\n",CopyFileUTF8("in.srw","out.srw") );
	//if(1==1)return 0;
	
	LONG lpSccCaps,pnCheckoutCommentLen,pnCommentLen;
	logFile=stdout;
	t=timer(t,"start");
	if(argc==2){
		char*wc=argv[1];
		
		THECONTEXT*ctx;
		
		SccInitialize( (LPVOID *) &ctx, NULL, "caller","scc", &lpSccCaps, "aux", &pnCheckoutCommentLen, &pnCommentLen);
		
		t=timer(t,"SccInitialize end");
		SccOpenProject(ctx, NULL, "root", wc, "pb-workspace", "aux", "comment", outproc,0);
		t=timer(t,"SccOpenProject end");
		
		printf("copy=%i\n",_copyfile(ctx,"in.srw","out.srw"));
		
		ScanWC(ctx,false);
		ScanWC(ctx,false);
	
		//for(i=0;i<ctx->svni->getCount();i++)ctx->svni->print(ctx->svni->get(i));
		//printf("count=%i\n\n\n",ctx->svni->getCount());
		
		SccUninitialize(ctx);
		
		
	}else{
		printf("specify local work copy as a parameter\n");
	}
	t=timer(t,"end");
	return 0;
}
