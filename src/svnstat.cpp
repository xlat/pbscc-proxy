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

/**
 * this code parses the response of the "svn status --xml" command using expat library
 */

 
#include <windows.h>
#include <stdio.h>
#include "expat.h"
#include "mstring.h"
#include "svninfo.h"
#include "pbscc.h"

#define BUF_SIZE	1024

#if defined(__amigaos__) && defined(__USE_INLINE__)
#include <proto/expat.h>
#endif

#ifdef XML_LARGE_SIZE
#if defined(XML_USE_MSC_EXTENSIONS) && _MSC_VER < 1400
#define XML_FMT_INT_MOD "I64"
#else
#define XML_FMT_INT_MOD "ll"
#endif
#else
#define XML_FMT_INT_MOD "l"
#endif


#define STPARSE_DEF     0
#define STPARSE_WC      1
#define STPARSE_REPO    2

typedef struct {
	int            status;     //the kind of status we will read STPARSE_* 
	mstring        target;     //target path (workcopy root)
	mstring        entry;      //entry path
	mstring        revision;   //committed revision
	mstring        wOwner;     //lock owner in the work copy
	mstring        rOwner;     //lock owner in the repository
	svninfo        *svni;      //the reference to the svninfo class that we will fill using parsed data
	mstring        cdata;      //string data value to hold xml text data
}XMLUDATA;

/** returns attribute value by it's name. null if attribute not found. */
const char * getAttribute(const char **attr,char * name){
	while(attr[0]){
		if(!strcmp(attr[0],name))return attr[1];
		attr+=2;
	}
	return NULL;
}

static void XMLCALL
startElement(void *userData, const char *name, const char **attr) {
	XMLUDATA * udata = (XMLUDATA *)userData;
	udata->cdata.set(NULL);  //reset value of the text data
	
	if( !strcmp(name,"target") ){
		udata->target.set( getAttribute(attr,"path") );
	}else if( !strcmp(name,"entry") ){
		udata->entry.set( getAttribute(attr,"path") );
		udata->wOwner.set( NULL );
		udata->rOwner.set( NULL );
		udata->revision.set( NULL );
		udata->status=STPARSE_DEF;
	}else if( !strcmp(name,"wc-status") ){
		udata->status=STPARSE_WC;
	}else if( !strcmp(name,"repos-status") ){
		udata->status=STPARSE_REPO;
	}else if( !strcmp(name,"commit") && udata->status==STPARSE_WC ){
		udata->revision.set(getAttribute(attr,"revision"));
	}
}

static void XMLCALL
endElement(void *userData, const char *name) {
	XMLUDATA * udata = (XMLUDATA *)userData;
	udata->cdata.rtrim();
	
	if( !strcmp(name,"owner") ){
		if( udata->status==STPARSE_WC ){
			udata->wOwner.set(udata->cdata.c_str());
		}else if( udata->status==STPARSE_REPO ){
			udata->rOwner.set(udata->cdata.c_str());
		}
	}else if( !strcmp(name,"entry") ){
		//store entry into svninfo class
		udata->svni->add(udata->target, udata->entry, NULL, udata->revision, udata->rOwner, udata->wOwner.len()>0 );
	}
	
	//clear data
	udata->cdata.set(NULL);
}

static void XMLCALL
dataHandler(void *userData, const XML_Char *s, int len) {
	XMLUDATA * udata = (XMLUDATA *)userData;
	udata->cdata.append(s,len);
}


bool parseSvnStatus(char*filename,svninfo*svni,mstring*err){
	int read;
	XMLUDATA udata;
	XML_Parser p = XML_ParserCreate(NULL);
	udata.status=STPARSE_DEF;
	udata.svni=svni;
	err->set(NULL);
	XML_SetElementHandler(p, startElement, endElement);
	XML_SetCharacterDataHandler(p, dataHandler);
	XML_SetEncoding(p,"UTF-8");

	XML_SetUserData(p, &udata);
	FILE*f=fopen(filename, "r");
	if(f){
		do{
			void *buf = XML_GetBuffer(p, BUF_SIZE);
			read=fread(buf,1,BUF_SIZE,f);
			if (! XML_ParseBuffer(p, read, /*isFinal*/(read == 0) )){
				//handle error here
				err->sprintf("%s at line %i\n", XML_ErrorString(XML_GetErrorCode(p)), XML_GetCurrentLineNumber(p));
			}
		}while(read>0 && err->len()==0);
		fclose(f);
	}else{
		err->sprintf("can't open file \"%s\"",filename);
	}
	XML_ParserFree(p);
	return (err->len()==0);
}


