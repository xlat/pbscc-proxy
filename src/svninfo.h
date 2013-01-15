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
 * this class defines simple class to hold svn object list
 */

 
 
#include "mstring.h"

#ifndef __SVNINFO_H__
#define __SVNINFO_H__
//initial size
#define __SVNINFO_H__INITSIZE	5000

#define __SVNINFO_ERR_PATH	1



typedef struct {
	int        hash;      // simple hash
	char       *path;     // relative path to file
	char       *rev;      // revision 
	char       *owner;    // object owner (locker)
	bool       isOwner;   // true if current user is the owner
}SVNINFOITEM;


class svninfo {
	private:
		SVNINFOITEM* ptr;
		long size;         
		long count;
		mstring*buf;
		
		int hash(const char*c){
			int h=0;
			char*ch="_";
			for (int i = 0; c[i]; i++) {
				ch[0]=c[i];
				CharLowerBuff(ch,1);
				h = 31*h + ch[0];
			}
			return h;
		}
		/** returns relative path of the _path comparing to _root */
		const char * relativePath(const char*_root,const char*_path){
			int len=strlen(_root); //length of the _root must be less or equal to _path
			if(len>0) {
				if(_root[len-1]=='\\' || _root[len-1]=='/')len--;
			}
			if( CompareString(LOCALE_USER_DEFAULT,NORM_IGNORECASE,_root,len,_path,len)==2 ){
				if(_path[len]=='\\' || _path[len]=='/' )return _path+len+1;
				if(!_path[len]) return _path+len;
			}
			//just return the full path
			return NULL;
		}
		
		
	public:
		
		svninfo(){
			size  = __SVNINFO_H__INITSIZE;
			count = 0;
			ptr   = new SVNINFOITEM[size];
			memset( ptr, 0, size*sizeof(SVNINFOITEM) );
			buf=new mstring();
		}
		
		~svninfo(){
			reset();
			delete []ptr;
			delete buf;
			ptr=NULL;
			count=0;
			size=0;
		}
		/**
		 * adds to the end of the list an item
		 * will not check if item already exists 
		 * @param _root: the base directory normally it's a root of work directory (used to calculate relative path)
		 * @param _path: the path to the element we want to add 
		 * @param _name: the name of the element we want to add (could be empty if _path contains the full path) 
		 * @param _rev : the revision of the element
		 * @param _owner: the lock owner of the element
		 * @param _isOwner: is current object is locked by current user
		 */
		void add(const char*_root,const char*_path,const char*_name,const char*_rev,const char*_owner,bool _isOwner){
			if( (_path=relativePath(_root,_path))==NULL)return;
			
			if(count+1>=size){
				//reallocate
				SVNINFOITEM *ptr_old=ptr;
				long size_old=size;
				size=size+size/2;
				ptr=new SVNINFOITEM[size];
				memset( ptr, 0, size*sizeof(SVNINFOITEM) );
				memcpy(ptr, ptr_old, size_old*sizeof(SVNINFOITEM));
				delete []ptr_old;
			}
			
			ptr[count].path=buf->set(_path)->addPath(_name)->c_copy();
			ptr[count].rev=buf->set(_rev)->c_copy("");
			ptr[count].owner=buf->set(_owner)->c_copy("");
			ptr[count].hash=hash(ptr[count].path);
			ptr[count].isOwner=_isOwner;
			count++;
		}
		
		/** reset all the values */
		void reset() {
			for(int i=0;i<count;i++){
				if(ptr[i].path)  delete []ptr[i].path;
				ptr[i].path=NULL;
				if(ptr[i].rev)  delete []ptr[i].rev;
				ptr[i].rev=NULL;
				if(ptr[i].owner) delete []ptr[i].owner;
				ptr[i].owner=NULL;
			}
			count=0;
		}
		
		void print(SVNINFOITEM*e,FILE*f){
			if(!e)fprintf(f,"\t(null)\n");
			else fprintf(f,"\t%s, %s, %s, %i\n",e->path, e->rev, e->owner, e->isOwner);
		}
		
		void print(FILE*f){
			if(f==NULL)return;
			fprintf(f,"SVNINFO>> (%i)\n",count);
			for(int i=0;i<count;i++){
				print(&ptr[i],f);
			}
			fprintf(f,"<<SVNINFO\n",count);
		}
		
		/** returns element count */
		int getCount(){
			return count;
		}
		
		/** returns element by index */
		SVNINFOITEM* get(int i){
			return &ptr[i];
		}
		
		/** returns svn element by relative path 
		SVNINFOITEM* get(char*_path){
			int h=hash(_path);
			for(int i=0;i<count;i++) {
				if( ptr[i].hash==h ){
					if ( !lstrcmpi( _path, ptr[i].path ) )return &ptr[i];
				}
			}
			return NULL;
		}
		*/
		
		/** returns svn element by absolute path with root specified 
		* @param err ptr to get error code. could be NULL
		*/
		SVNINFOITEM* get(const char*_root, const char*_path,int*err){
			if( (_path=relativePath(_root,_path))==NULL ){
				if(err)err[0]=__SVNINFO_ERR_PATH;
				return NULL;
			}
			if(err)err[0]=0;
			int h=hash(_path);
			for(int i=0;i<count;i++) {
				if( ptr[i].hash==h ){
					if ( !lstrcmpi( _path, ptr[i].path ) )return &ptr[i];
				}
			}
			return NULL;
		}
		
		bool setIsOwner(const char*_root, const char*_path,bool b){
			int err=0;
			SVNINFOITEM * item=get(_root,_path,&err);
			if(item){
				item->isOwner=b;                  //set or remove isOwner flag for a file
				if(item->owner)item->owner[0]=0;  //remove owner name in any case
			}else if(err){
				return false;
			}
			return true;
		}
	
};	


#endif
