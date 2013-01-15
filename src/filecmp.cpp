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

bool filecmp(const char*name1,const char*name2) {
	FILE *fp1, *fp2;
	int ch1=1;
	int ch2=2;

	/* open first file */
	if((fp1 = fopen(name1, "rb"))!=NULL) {
		/* open second file */
		if((fp2 = fopen(name2, "rb"))!=NULL) {
			/* compare the files */
			do{
				ch1=fgetc(fp1);
				ch2=fgetc(fp2);
			}while( ch1==ch2 && ch1!=EOF);
			
			fclose(fp2);
		}
		fclose(fp1);
	}
	return (ch1==ch2);
}
