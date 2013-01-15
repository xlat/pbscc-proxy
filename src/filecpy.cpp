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
#include <stdlib.h>
#include <tchar.h>


#define HASTATE_START	0
#define HASTATE_ASCII	1
#define HASTATE_LEN		2
#define HASTATE_HA		3
#define HASTATE_END		4





/**
 * Copies HEXASCII file to UTF8 file
 */

BOOL CopyFileUTF8(const char*src,char*dst){
//	BOOL b=false;
//	return b;
	int ch,b4,i;
	///buffer for reading and comparing data
	char buf[50]; //only ascii chars are here
	int buflen=0;
	///shift for hex unicode chars
	int b4shift[]={4,0,12,8};
	
	int state=0;
	int halen; //number of hexascii chars
	
	FILE*fin=NULL;
	FILE*fout=NULL;
	
	fin=fopen(src,"rb");
	fout=fopen(dst,"wb");
	
	if(fin==NULL)goto err;
	if(fout==NULL)goto err;
	
	//write out UTF-8 BOM
	putc(0xEF,fout);
	putc(0xBB,fout);
	putc(0xBF,fout);
	
	while( (ch = getc( fin )) != EOF ) {
		if(state==HASTATE_START){
			//HEXASCII file must start with "HA" chars
			if(ch!='H')goto err;
			ch=getc( fin );
			if(ch!='A')goto err;
			state=HASTATE_ASCII;
		}else if(state==HASTATE_ASCII){
			//waiting for pure ascii chars and maybe for a $$HEX prefix
			buf[buflen]=ch;
			buflen++;
			if(!strncmp( buf, "$$HEX",buflen )){
				if(buflen==5){
					state=HASTATE_LEN;
					halen=0;
					buflen=0;
				}
			}else if(buflen==3 && !strncmp( buf, "$$$",buflen )) {
				//write out one $ and keep in buffer two onces "$$"
				fwrite( buf, 1 /*elem size*/, 1 /*elem count*/, fout );
				buflen--;
			}else{
				//move everything from buf into out file
				fwrite( buf, buflen /*elem size*/, 1 /*elem count*/, fout );
				buflen=0;
			}
		}else if(state==HASTATE_LEN){
			//here there should be a length of the HASCII sequence
			if(ch>='0' && ch<='9'){
				//halen is in decimal format
				halen=halen*10+(ch-'0');
			}else if(ch=='$'){
				//there should be the next $ char
				ch=getc( fin );
				if(ch!='$')goto err;
				state=HASTATE_HA;
			}else{
				goto err;
			}
		}else if(state==HASTATE_HA){
			//here heximal representation of the unicode chars
			int wch=0;
			for(i=0;i<4;i++){
				switch(ch){
					case '0':b4=0;break;
					case '1':b4=1;break;
					case '2':b4=2;break;
					case '3':b4=3;break;
					case '4':b4=4;break;
					case '5':b4=5;break;
					case '6':b4=6;break;
					case '7':b4=7;break;
					case '8':b4=8;break;
					case '9':b4=9;break;
					case 'a':
					case 'A':b4=10;break;
					case 'b':
					case 'B':b4=11;break;
					case 'c':
					case 'C':b4=12;break;
					case 'd':
					case 'D':b4=13;break;
					case 'e':
					case 'E':b4=14;break;
					case 'f':
					case 'F':b4=15;break;
					default :goto err;
				}
				wch=wch | (b4<<b4shift[i]);
				if(i!=3)ch=getc( fin );
			}
			buflen=WideCharToMultiByte(CP_UTF8,0,(WCHAR*)&wch,1,buf,sizeof(buf),NULL,NULL);
			if(buflen>0){
				//move utf8 chars from buf into out file
				fwrite( buf, buflen /*elem size*/, 1 /*elem count*/, fout );
				buflen=0;
			}else {
				goto err;
			}
			halen--;
			
			if(halen<=0){
				//there should be $$ENDHEX$$ 
				buflen=fread( buf,1,10,fin );
				if(buflen!=10)goto err;
				if(strncmp(buf,"$$ENDHEX$$",buflen))goto err;
				buflen=0;
				state=HASTATE_ASCII;
			}
		}
	}
	
	fclose(fin);
	fflush(fout);
	fclose(fout);
	return true;
	
	err:
	if(fin)fclose(fin);
	if(fout)fclose(fout);
	
	return false;
	
}
/*
TCHAR*DELIMHEXASCII	=_T("$$");
TCHAR*OPENHEXASCII	=_T("$$HEX");
TCHAR*CLOSEHEXASCII	=_T("$$ENDHEX$$");

BOOL HADecode(TCHAR * buf){
	TCHAR *p=buf;
	TCHAR *p2;
	int ch,b8;
	int b8shift[]={4,0,12,8};
	int count,i;
	while ((p = _tcsstr(p, OPENHEXASCII)) != NULL) {
		p2 = _tcsstr(p+_tcslen(OPENHEXASCII), DELIMHEXASCII);
		if(!p2)return false;
		p2[0]=0;
		count=_ttol(p+_tcslen(OPENHEXASCII));
		if(count==0)return false;
		p2[0]=DELIMHEXASCII[0];
		p2+=_tcslen(DELIMHEXASCII);
		while(count){
			ch=0;
			for(i=0;i<4;i++){
				switch(p2[0]){
					case '0':b8=0;break;
					case '1':b8=1;break;
					case '2':b8=2;break;
					case '3':b8=3;break;
					case '4':b8=4;break;
					case '5':b8=5;break;
					case '6':b8=6;break;
					case '7':b8=7;break;
					case '8':b8=8;break;
					case '9':b8=9;break;
					case 'a':
					case 'A':b8=10;break;
					case 'b':
					case 'B':b8=11;break;
					case 'c':
					case 'C':b8=12;break;
					case 'd':
					case 'D':b8=13;break;
					case 'e':
					case 'E':b8=14;break;
					case 'f':
					case 'F':b8=15;break;
					default: return false;
				}
				ch=ch | (b8<<b8shift[i]);
				p2++;
			}
			p[0]=ch;
			p++;
			count--;
		}
		if(_tcsncmp(CLOSEHEXASCII,p2,_tcslen(CLOSEHEXASCII)))return false;
		_tcscpy(p,p2+_tcslen(CLOSEHEXASCII));
	}
	
	return true;
}
*/
