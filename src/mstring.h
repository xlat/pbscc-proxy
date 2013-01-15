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

#ifndef __MSTRING_H__
#define __MSTRING_H__
#define __MSTRING_H__ALLOC 512

#include <tchar.h>
#include "deelx.h"

#define ASSERT(x) {if(!(x)) _asm{int 0x03}}
#define TCHAR_ARG   TCHAR
#define WCHAR_ARG   WCHAR
#define CHAR_ARG    char
#define DOUBLE_ARG  double

#define FORCE_ANSI      0x10000
#define FORCE_UNICODE   0x20000
#define FORCE_INT64     0x40000

class mstring {
	private:
		//--- member variables

		TCHAR* ptr;
		long allocated;
		long length;
		
		void init(){
			allocated=0;
			length=0;
			ptr = NULL;
			//printf("init\n");
		}
		//! Reallocate the array, make it bigger or smaler
		void realloc(long new_size) {
			//printf("realloc > %i\n",new_size);
			if(new_size>allocated){
				TCHAR* old_ptr = ptr;
				new_size/=__MSTRING_H__ALLOC;
				new_size++;
				new_size*=__MSTRING_H__ALLOC;
				ptr=new TCHAR[new_size];
				if(old_ptr)	{
					_tcscpy(ptr,old_ptr);
					delete [] old_ptr;
				}else{
					ptr[0]=0;
				}
				allocated=new_size;
				//printf("realloc >> %i\n",new_size);
			}
		}
	public:
		//! Default constructor
		mstring(){
			init();
		}

		//! Constructor
		mstring(const TCHAR*c){
			init();
			append(c);
		}


		//! destructor
		~mstring() {
			if(ptr)delete [] ptr;
			//printf("destroy\n");
		}
		
		// operator to cast to a TCHAR*
		operator TCHAR *() {
			return ptr;
		}
		
		TCHAR * c_str(){
			return ptr;
		}
		
		/** returns the copy of the buffer allocated with new[] operator
		 *  you must free this allocated memory with delete[] operator
		 */
		TCHAR * c_copy(TCHAR*_def=NULL){
			if(ptr==NULL && _def==NULL)return NULL;
			long i=length;
			TCHAR *p=ptr;
			
			if(p==NULL){
				p=_def;
				i=_tcslen(p);
			}
			TCHAR *cpy=new TCHAR[i+1];
			_tcscpy(cpy,p);
			return cpy;
		}
		
		//flag could be NORM_IGNORECASE
		bool startsWith(const TCHAR *c, int flag=0){
			int ret;
			if(flag & NORM_IGNORECASE)
				ret=_tcsnicmp(ptr, c, _tcslen(c));
			else
				ret=_tcsnccmp(ptr, c, _tcslen(c));
				
			if(!ret)return true;
			return false;
		}
		
		TCHAR charAt(long i){
			if(i>length || i<0 || length==0)return 0;
			return ptr[i];
		}

		mstring * set(const TCHAR * c){
			if(c){
				long i=_tcslen(c);
				realloc(i+1);
				_tcscpy(ptr,c);
				length=i;
				//printf("set > %s<<\n",ptr);
			}else{
				if(ptr){
					delete [] ptr;
					ptr=NULL;
				}
				realloc(0);
				length=0;
				allocated=0;
				//printf("set > %s<<\n",ptr);
			}
			return this;
		}
		
		bool getenv(TCHAR * name){
			int len=GetEnvironmentVariable(name,NULL,0);
			if(len>0){
				realloc(len+1);
				GetEnvironmentVariable(name,ptr,len+1);
				length=_tcslen(ptr);
			}else{
				set(NULL);
				return false;
			}
			return true;
		}
		
		mstring* append(const TCHAR * c){
			if(!c)return this;
			long i=_tcslen(c);
			realloc(length+i+1);
			_tcscpy(ptr+length,c);
			length+=i;
			//printf("append > %s<<\n",ptr);
			return this;
		}
		
		mstring* append(const TCHAR * c,int len){
			if(!c || !len)return this;
			realloc(length+len+1);
			_tcsncpy(ptr+length,c,len);
			length+=len;
			ptr[length]=0;
			length=_tcslen(ptr);
			//printf("append > %s<<\n",ptr);
			return this;
		}
		
		mstring* append(TCHAR c){
			if(!c)return this;
			realloc(length+2); //1 char + 1 EOS
			ptr[length]=c;
			length++;
			ptr[length]=0;
			return this;
		}
		
		/** removes first char from the string. 
		  * returns reference to itself.
		  */
		mstring* dequeue(){
			if(length>0){
				_tcscpy(ptr,ptr+1);
				length--;
			}
			return this;
		}
		
		mstring* getIniString(const TCHAR * section,const TCHAR * key,const TCHAR * def,const TCHAR * filename ){
			GetPrivateProfileString(section, key, def, ptr, allocated, filename );
			length=_tcslen(ptr);
			return this;
		}
		
		
		long len(){
			return length;
		}

		mstring* toDir(){
			if(length>0 && ptr[length-1]!='/' && ptr[length-1]!='\\' )append(_T("\\"));
			return this;
		}
		
		mstring* addPath(const TCHAR * c){
			if( c && c[0] ){
				toDir();
				append(c);
			}
			return this;
		}
		
		mstring* rtrim(){
			if(length==0)return this;
			int len=length;
			TCHAR*space=_T(" \t\r\n");
			for(int i=length-1; i>=0 && _tcschr(space,ptr[i]) ; i--)
				ptr[i]=0;
			length=_tcslen(ptr);
			return this;
		}
		/**
		 * Truncates string to specified length, returns the new length.
		 * if newlen<0 then truncate starts from last char
		 * "abc".trunc(-1) = "ab"
		 */
		mstring* trunc(long newlen){
			if(newlen<0)newlen = (length+newlen>0)?(length+newlen):0 ;
			if(newlen<length){
				length=newlen;
				ptr[length]=0;
			}
			return this;
		}
		
		
		mstring* getWindowText(HWND hwnd, int dlgItem){
			length=SendDlgItemMessage(hwnd,dlgItem,WM_GETTEXTLENGTH,0,0);
			realloc(length+1);
			SendDlgItemMessage(hwnd,dlgItem,WM_GETTEXT,length+1,(LPARAM) ptr);
			return this;
		}

		
		void vsprintf(LPCTSTR lpszFormat, va_list argList){

			va_list argListSave = argList;

			// make a guess at the maximum length of the resulting string
			int nMaxLen = 0;
			for (LPCTSTR lpsz = lpszFormat; *lpsz != '\0'; lpsz = _tcsinc(lpsz))
			{
				// handle '%' character, but watch out for '%%'
				if (*lpsz != '%' || *(lpsz = _tcsinc(lpsz)) == '%')
				{
					nMaxLen += _tclen(lpsz);
					continue;
				}

				int nItemLen = 0;

				// handle '%' character with format
				int nWidth = 0;
				for (; *lpsz != '\0'; lpsz = _tcsinc(lpsz))
				{
					// check for valid flags
					if (*lpsz == '#')
						nMaxLen += 2;   // for '0x'
					else if (*lpsz == '*')
						nWidth = va_arg(argList, int);
					else if (*lpsz == '-' || *lpsz == '+' || *lpsz == '0' ||
						*lpsz == ' ')
						;
					else // hit non-flag character
						break;
				}
				// get width and skip it
				if (nWidth == 0)
				{
					// width indicated by
					nWidth = _ttoi(lpsz);
					for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz = _tcsinc(lpsz))
						;
				}
				ASSERT(nWidth >= 0);

				int nPrecision = 0;
				if (*lpsz == '.')
				{
					// skip past '.' separator (width.precision)
					lpsz = _tcsinc(lpsz);

					// get precision and skip it
					if (*lpsz == '*')
					{
						nPrecision = va_arg(argList, int);
						lpsz = _tcsinc(lpsz);
					}
					else
					{
						nPrecision = _ttoi(lpsz);
						for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz = _tcsinc(lpsz))
							;
					}
					ASSERT(nPrecision >= 0);
				}

				// should be on type modifier or specifier
				int nModifier = 0;
				if (_tcsncmp(lpsz, _T("I64"), 3) == 0)
				{
					lpsz += 3;
					nModifier = FORCE_INT64;
#if !defined(_X86_) && !defined(_ALPHA_)
					// __int64 is only available on X86 and ALPHA platforms
					ASSERT(FALSE);
#endif
				}
				else
				{
					switch (*lpsz)
					{
					// modifiers that affect size
					case 'h':
						nModifier = FORCE_ANSI;
						lpsz = _tcsinc(lpsz);
						break;
					case 'l':
						nModifier = FORCE_UNICODE;
						lpsz = _tcsinc(lpsz);
						break;

					// modifiers that do not affect size
					case 'F':
					case 'N':
					case 'L':
						lpsz = _tcsinc(lpsz);
						break;
					}
				}

				// now should be on specifier
				switch (*lpsz | nModifier)
				{
				// single characters
				case 'c':
				case 'C':
					nItemLen = 2;
					va_arg(argList, TCHAR_ARG);
					break;
				case 'c'|FORCE_ANSI:
				case 'C'|FORCE_ANSI:
					nItemLen = 2;
					va_arg(argList, CHAR_ARG);
					break;
				case 'c'|FORCE_UNICODE:
				case 'C'|FORCE_UNICODE:
					nItemLen = 2;
					va_arg(argList, WCHAR_ARG);
					break;

				// strings
				case 's':
					{
						LPCTSTR pstrNextArg = va_arg(argList, LPCTSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6;  // "(null)"
						else
						{
						   nItemLen = lstrlen(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
					}
					break;

				case 'S':
					{
#ifndef _UNICODE
						LPWSTR pstrNextArg = va_arg(argList, LPWSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6;  // "(null)"
						else
						{
						   nItemLen = wcslen(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
#else
						LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6; // "(null)"
						else
						{
						   nItemLen = lstrlenA(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
#endif
					}
					break;

				case 's'|FORCE_ANSI:
				case 'S'|FORCE_ANSI:
					{
						LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6; // "(null)"
						else
						{
						   nItemLen = lstrlenA(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
					}
					break;

				case 's'|FORCE_UNICODE:
				case 'S'|FORCE_UNICODE:
					{
						LPWSTR pstrNextArg = va_arg(argList, LPWSTR);
						if (pstrNextArg == NULL)
						   nItemLen = 6; // "(null)"
						else
						{
						   nItemLen = wcslen(pstrNextArg);
						   nItemLen = max(1, nItemLen);
						}
					}
					break;
				}

				// adjust nItemLen for strings
				if (nItemLen != 0)
				{
					if (nPrecision != 0)
						nItemLen = min(nItemLen, nPrecision);
					nItemLen = max(nItemLen, nWidth);
				}
				else
				{
					switch (*lpsz)
					{
					// integers
					case 'd':
					case 'i':
					case 'u':
					case 'x':
					case 'X':
					case 'o':
						if (nModifier & FORCE_INT64)
							va_arg(argList, __int64);
						else
							va_arg(argList, int);
						nItemLen = 32;
						nItemLen = max(nItemLen, nWidth+nPrecision);
						break;

					case 'e':
					case 'g':
					case 'G':
						va_arg(argList, DOUBLE_ARG);
						nItemLen = 128;
						nItemLen = max(nItemLen, nWidth+nPrecision);
						break;

					case 'f':
						{
							// 312 == strlen("-1+(309 zeroes).")
							// 309 zeroes == max precision of a double
							// 6 == adjustment in case precision is not specified,
							//   which means that the precision defaults to 6
							va_arg(argList, double);
							nItemLen = max(nWidth, 312)+nPrecision+6;
						}
						break;

					case 'p':
						va_arg(argList, void*);
						nItemLen = 32;
						nItemLen = max(nItemLen, nWidth+nPrecision);
						break;

					// no output
					case 'n':
						va_arg(argList, int*);
						break;

					default:
						ASSERT(FALSE);  // unknown formatting option
					}
				}

				// adjust nMaxLen for output nItemLen
				nMaxLen += nItemLen;
			}

			realloc(length + nMaxLen +1 );
			::_vstprintf(ptr+length, lpszFormat, argListSave);
			length=_tcslen(ptr);

			va_end(argListSave);
		}
		
		
		mstring* sprintf(TCHAR* format, ...){
			va_list argList;
			va_start(argList, format);
			vsprintf(format, argList);
			va_end(argList);
			return this;
		}

		/* 
			enum REGEX_FLAGS {
				NO_FLAG        = 0,
				SINGLELINE     = 0x01,
				MULTILINE      = 0x02,
				GLOBAL         = 0x04,
				IGNORECASE     = 0x08,
				RIGHTTOLEFT    = 0x10,
				EXTENDED       = 0x20
			};
		*/
		bool match(TCHAR*pattern,REGEX_FLAGS flags=NO_FLAG){
		    // declare
            CRegexp regexp(pattern,flags);
            // test
            MatchResult result = regexp.Match(ptr);
            // matched or not
            return result.IsMatched()!=0;
		}

};



#endif

