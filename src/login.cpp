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

//login dialog functions


BOOL CALLBACK DialogProcLogin(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam){
	switch(msg){
		case WM_MANAGEOK:{
				THECONTEXT*ctx;
				ctx=(THECONTEXT*)GetWindowLong(hwnd,GWL_USERDATA);
				BOOL enableOK;
				
				enableOK = ( SendDlgItemMessage(hwnd,IDC_SVNUID,WM_GETTEXTLENGTH,0,0) >0 );

				EnableWindow( GetDlgItem(hwnd,IDOK)  , enableOK );
				break;
			}
		case WM_CLOSE:
			EndDialog(hwnd,0);
			break;
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
					
					SendDlgItemMessage(hwnd,IDC_SVNUID,WM_GETTEXT,PBSCC_UID+1,(LPARAM) ctx->uid);
					SendDlgItemMessage(hwnd,IDC_SVNPWD,WM_GETTEXT,PBSCC_UID+1,(LPARAM) ctx->pwd);
					//close window
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
			}
			return 1;   
		case WM_INITDIALOG:{
			THECONTEXT *ctx;
			ctx=(THECONTEXT*)lParam ;
			ctx->uid[0]=0;
			ctx->pwd[0]=0;
			SetWindowLong(hwnd,	GWL_USERDATA,(LONG) lParam);
			SendDlgItemMessage(hwnd,IDC_SVNUID,EM_LIMITTEXT,PBSCC_UID,0);
			SendDlgItemMessage(hwnd,IDC_SVNPWD,EM_LIMITTEXT,PBSCC_UID,0);
			//manage OK button 
			//PostMessage(hwnd,WM_MANAGEOK,0,0);
			
			return 1;
		}
	}
	return 0;
}

bool _loginscc(THECONTEXT*ctx){
	log("login\n");
	DialogBoxParam(hInstance,MAKEINTRESOURCE(IDD_DIALOG_LOGIN),ctx->parent,&DialogProcLogin,(LPARAM)ctx);
	return ctx->uid[0]!=0;
}

