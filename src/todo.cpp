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
#include "pbscc.h"
#include <io.h>

//Functions to add checked out objects into PB "TODO" window
//dlukyanov: I don't like this code at all 



//returns a pointer to character after last '\\' or '/' symbol
//returns null if not found
char*getFileName(char*c){
	char*s=NULL;
	for(int i=0;c[i]!=0;i++)if(c[i]=='\\')s=c+i+1;
	return s;
}



bool PBGetVersion(char version[MAX_PATH+1]){
	bool ret=false;
	HKEY hKey;
	HKEY hKeyHelp;
	DWORD dwIndex=0;
	char path[MAXFULLPATH];
	char helpkey[MAX_PATH+20];
	char * fname;
	if(!GetModuleFileName(NULL,path,sizeof(path)))return ShowSysError("GetModuleFileName");
	if( (fname=getFileName(path))!=NULL )fname[0]=0;
	printf("path=%s\n",path);
	if(RegOpenKey(HKEY_LOCAL_MACHINE,"SOFTWARE\\SYBASE\\PowerBuilder",	&hKey)!=ERROR_SUCCESS)return ShowSysError("RegOpenKey");
	while(!ret && RegEnumKey( hKey, dwIndex, version,MAX_PATH+1)==ERROR_SUCCESS){
		strcpy(helpkey,version);
		strcat(helpkey,"\\Help");
		if(RegOpenKey(hKey,helpkey,&hKeyHelp)==ERROR_SUCCESS){
			DWORD lpType=REG_SZ;
			char helppath[MAXFULLPATH];
			DWORD cbData=sizeof(helppath);
			if(RegQueryValueEx(hKeyHelp,"",NULL,&lpType,(BYTE*)helppath,&cbData)==ERROR_SUCCESS){
				if( (fname=getFileName(helppath))!=NULL )fname[0]=0;
				if(!stricmp( path, helppath ))ret=true;
			}
			RegCloseKey(hKeyHelp);
		}
		dwIndex++;
	}
	RegCloseKey(hKey);
	return ret;
}

//returns *.pbl file name where object is stored
bool GetPBLFile(char* objectPath, char* pblFileName, int pblFileNameSize) {           			
	char* objectName;	
	int currentEndPos = -1;
	
	//getting object file name
	int i;
	int z = 0;
	for(i = 0; objectPath[i] != '\0'; i++){
		if(objectPath[i] == '\\')
			currentEndPos = i;
	}

	if(currentEndPos == -1) 
		return false;

	objectName = &objectPath[currentEndPos + 1];

	//replacing the objectName with lowercase characters
	strlwr(objectName);	   

	char* currentPath = new char[ currentEndPos + MAX_PATH + 2 ]; 

	//getting path of object file
	strncpy(currentPath, objectPath, currentEndPos + 1);
	currentPath[ currentEndPos + 1 ] = '\0';


	//finding of *.pbg files in current directory
	struct _finddata_t  fileinfo;
	long                handle;
	int                 rc;
		
	handle = _findfirst(strcat(currentPath,"*.pbg"), &fileinfo);
	rc = handle;
	while(rc != -1 ) {      
		currentPath[ currentEndPos + 1 ] = '\0';

		strcat(currentPath, fileinfo.name);

		FILE * file;
		file = fopen((LPCTSTR) currentPath, "r");

		if(file != NULL) {
			char buffer[1000];
					   
			while(fgets(buffer, sizeof(buffer), file) != NULL){
				char * ptr = strstr( strlwr(buffer), objectName);
				if(ptr != NULL)	{
								 
					if( 	ptr != buffer && 
						(ptr[-1] == '\\' || ptr[-1] == '"') &&  
						ptr[strlen(objectName)] == '"' &&
						currentEndPos + (int)strlen(fileinfo.name) + 2 < pblFileNameSize) 
					{
						strcpy(pblFileName, currentPath);
						pblFileName[strlen(pblFileName) - 1] = 'l';

						delete [] currentPath;
						return true;
					}
				}
			}
			fclose(file);
		}else{
			//printf("\nGetPBLFile: ERROR\n");
		}

		rc = _findnext( handle, &fileinfo );
		}
	_findclose( handle );

	delete [] currentPath;
	return false;
}

 //retunrs index of new line to add into ToDoList
int ToDoGetIndex(char* subToDoKey, char* newToDoValue){
	int index;
	int ret;
	HKEY hToDo;
	int counter = 0;

	if( RegCreateKeyEx(	HKEY_CURRENT_USER,  
				(LPCTSTR) subToDoKey, 
				0, NULL, NULL, 
				KEY_READ, //KEY_ALL_ACCESS, 
				NULL,
				&hToDo,	
				NULL)  == ERROR_SUCCESS)
	{ 			
		//getting number of lines in ToDoList
		DWORD maxToDoValueLen;
	 
		if( RegQueryInfoKey(	hToDo, 
					NULL, NULL, 
					NULL, 
					NULL, NULL, NULL, NULL, NULL, 
					& maxToDoValueLen, 
					NULL, NULL) != ERROR_SUCCESS)
		{ 			
			return -1;
		} 	

		int maxLen = maxToDoValueLen; 

		maxToDoValueLen += 1;
		char* toDoData = new char[maxToDoValueLen]; 
					   
		if( RegQueryValueEx(	hToDo, 
					(LPCTSTR) "Count", 
					NULL, NULL, 
					(BYTE*) toDoData,
					& maxToDoValueLen) != ERROR_SUCCESS)
		{
			ret = RegCloseKey(hToDo);
			delete [] toDoData;
			return 0;
		} 		
	
		counter = atoi(toDoData);
		index = counter;

		//searching of line which is equal to line that we want to add in ToDoList
		int i;
		for(i = 0; i < counter; i++){
			char buffer[20];
			maxToDoValueLen = maxLen;
		
			if( RegQueryValueEx(	hToDo, 
						(LPCTSTR) itoa( i, buffer, 10 ),
						NULL, NULL,
						(BYTE*) toDoData,
						& maxToDoValueLen) != ERROR_SUCCESS)
			{ 			
				ret = RegCloseKey(hToDo);
				delete [] toDoData;			
				return -1;
			} 		

			if(strcmpi(toDoData + 1, newToDoValue + 1) == 0) {	
				index = i;                            
			}//else printf("\nGetIndex: lines are not equal ...");
		}                                  
		ret = RegCloseKey(hToDo);

		delete [] toDoData;
	}else{
		return -1;
	}
	return index; 
}

//sets new line into ToDoList
//NY= y to cross the line; n not to cross the line
//target - the path to the powerbuilder target file
//objectFileName - path to powerbuilder exported object file
//versionNumber - current powerbuilder version number retrurned by PBGetVersion
bool ToDoSetLine(char * NY, char * target,  char * objectFileName, char * versionNumber){
	if(!target || !target[0])return false;
	if(!versionNumber || !versionNumber[0])return false;
	
	int i;
	int objectFileNameLen = strlen(objectFileName);

		//finding *.pbl file name where object is stored
	char pblFileName[MAXFULLPATH]; 
	char *pblName; 
				   
	if( !GetPBLFile(objectFileName, pblFileName, MAXFULLPATH) ) return false;
	pblName=getFileName(pblFileName);
	if(!pblName) return false;
		 
	//finding name of object in objectFileName string
	char * objectName;
	int slashPos = -1;
	int pointPos = -1;
	for(i = 0; objectFileName[i] != '\0'; i++){
		if(objectFileName[i] == '\\')
			slashPos = i;
		if(objectFileName[i] == '.')
			pointPos = i;
	}
	if(slashPos == -1 || pointPos == -1)return false;

	int objectLen = pointPos - slashPos - 1;
	objectName = &objectFileName[slashPos + 1];

	char* newToDoValue = new char[27 + 20 + (2 * objectLen) + strlen(pblName) + 
					strlen(pblFileName) + strlen(target)];

	printf("\nSize newToDoValue %d", 27 + 20 + (2 * objectLen) + strlen(pblName) + 
					strlen(pblFileName) + strlen(target));
		
	//generating line for inserting into data field in registry
	strcpy(newToDoValue, NY);
	strcat(newToDoValue, "\t");
	strncat(newToDoValue, objectName, objectLen);
	strcat(newToDoValue, " (");
	strcat(newToDoValue, pblName);	
	strcat(newToDoValue, ")");
	strcat(newToDoValue, "\t");

	switch (objectFileName[ objectFileNameLen - 1 ])
	{
		case 'd':	strcat(newToDoValue, "datawindow");	break;
		case 'a':	strcat(newToDoValue, "application"); 	break;
		case 'w':	strcat(newToDoValue, "window");      	break;
		case 'm':	strcat(newToDoValue, "menu");		break;
		case 'u':	strcat(newToDoValue, "userobject");   	break;
		case 'f':	strcat(newToDoValue, "function");    	break;
		case 'x':	strcat(newToDoValue, "proxy");       	break;
		case 's':	strcat(newToDoValue, "structure");    	break;
		case 'p':	strcat(newToDoValue, "pipeline");    	break;
		case 'j':	strcat(newToDoValue, "project");   	break;
		case 'q':	strcat(newToDoValue, "query");      	break;

		default: delete [] newToDoValue; return false; 
	}

	strcat(newToDoValue, ":///");

	//formatting of *.pbl file name
	for(i = 0; pblFileName[i] != '\0'; i++){
		if(pblFileName[i] == ':')pblFileName[i] = '|';
		if(pblFileName[i] == '\\')pblFileName[i] = '/';
	}

	strcat(newToDoValue, pblFileName);

	//formatting of *.pbl file name
	for(i = 0; pblFileName[i] != '\0'; i++){
		if(pblFileName[i] == '|')pblFileName[i] = ':';
		if(pblFileName[i] == '/')pblFileName[i] = '\\';
	}

	strcat(newToDoValue, "?action=open&entry=");   
	strncat(newToDoValue, objectName, objectLen);
	strcat(newToDoValue, "\t");
	strcat(newToDoValue, target);

	  
	//closing To Do List
	char toDoListClassName[] = "CToDoList";
	HWND hToDoList;	
	
	while(true){
		hToDoList = FindWindow(toDoListClassName, NULL);
		if(hToDoList == NULL) break;
		else SendMessage(hToDoList, WM_CLOSE, 0, 0);
	}
	

	int ret = 0;
	HKEY hKey;
	char buffer[20];

	//generating access key of data field in registry to insert line 
	for(i = 0; target[i] != '\0'; i++){
		if(target[i] == '\\')   target[i] = '$';
	}
	//printf("\nSetToDoLine: target =  %s", target);
		
	char* subToDoKey = new char[50 + strlen(versionNumber) + strlen(target)];
	
	strcpy(subToDoKey, "Software\\Sybase\\PowerBuilder\\");
	strcat(subToDoKey, versionNumber);
	strcat(subToDoKey, "\\Target\\");
	strcat(subToDoKey, target);
	strcat(subToDoKey, "\\ToDo");
	
	for(i = 0; target[i] != '\0'; i++){
		if(target[i] == '$')   	target[i] = '\\';
	}
	//printf("\nSetToDoLine: subToDoKey = %s", subToDoKey);

	//finding correct number for adding a new line in ToDoList
	int indexToInsertLine = ToDoGetIndex(subToDoKey, newToDoValue);

	if(indexToInsertLine == -1){
		delete [] newToDoValue;
		delete [] subToDoKey;
		return false;
	}
	
	//getting of Count value from registry
	if( RegCreateKeyEx(	HKEY_CURRENT_USER,  
				(LPCTSTR) subToDoKey, 
				0, NULL, NULL,
				KEY_ALL_ACCESS, 
				NULL,&hKey,NULL) == ERROR_SUCCESS)
	{
		DWORD buflen=sizeof(buffer);
		if( RegQueryValueEx(	hKey, 
					(LPCTSTR) "Count", 
					NULL, NULL, 
					(BYTE*) buffer,
					& buflen)  != ERROR_SUCCESS)
		{
			strcpy(buffer,"0");
		}

		int counter = atoi(buffer);

		if(indexToInsertLine == counter) 
			counter++;

		//inserting new line in ToDoList
		ret = RegSetValueEx(	hKey, 
					(LPCTSTR) itoa(indexToInsertLine, buffer, 10),
					0,
					REG_SZ, 
					(CONST BYTE*) newToDoValue,
					(DWORD) strlen(newToDoValue) + 1);

		ret = RegSetValueEx(	hKey, 
					(LPCTSTR) "Count", 
					0,
					REG_SZ,
					(CONST BYTE*) itoa(counter, buffer, 10),
					(DWORD) strlen(itoa(counter, buffer, 10)) + 1);

		ret = RegSetValueEx(	hKey, 
					(LPCTSTR) "Selection", 
					0, 
					REG_SZ,
					(CONST BYTE*) itoa(indexToInsertLine, buffer, 10),
					(DWORD) strlen(itoa(indexToInsertLine, buffer, 10)) + 1);
	
		ret = RegCloseKey(hKey);
	}else{
		delete [] newToDoValue;
		delete [] subToDoKey;
		return false;
	}
	delete [] newToDoValue;
	delete [] subToDoKey;
																		  
	return true;	
}

