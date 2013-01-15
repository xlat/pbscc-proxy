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

//simple string functions

//removes right spaces
char*rtrim(char*c){
	char*space=" \t\r\n";
	int len=strlen(c);
	for(int i=len-1;i>=0;i--){
		if(strchr(space,c[i]))c[i]=0;
		else break;
	}
	return c;
}

//returns pointer to the first non-space character of the string (char*)
char * ltrim(char*c){
	while(c[0] && (c[0]==' ' || c[0]=='\t'))c++;
	return c;
}


