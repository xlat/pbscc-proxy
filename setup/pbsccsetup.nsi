;

!include "EnvVarUpdate.nsh"

SetCompressor lzma


Name "PBSCC Proxy $%PBSCC_VERSION%"

OutFile "pbsccsetup.exe"

; The default installation directory
InstallDir $PROGRAMFILES\TortoiseSVN

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "SOFTWARE\TortoiseSVN" "Directory"



; Pages

Page components

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles



Section "PBSCC Proxy"
	SectionIn RO
	SetRebootFlag false
	SetOutPath $INSTDIR\Bin
	File "Bin\svnci.cmd"
	File "Bin\svnlog.cmd"
	File "Bin\svndiff.cmd"
	File "Bin\pbscc.dll"
	
	WriteUninstaller "PbSccDel.exe"

	
	; Write the installation path into the registry
	WriteRegStr HKLM "SOFTWARE\TortoiseSVN" "Directory" "$INSTDIR"
	
	; Register scc
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "SCCServerName" "PBSCC Proxy"
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "SCCServerPath" "$INSTDIR\bin\pbscc.dll"
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "log.path" ""
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "noDDB" "1"
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "checkout.lock" "0"
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "cache.ttl.seconds" "60"
	WriteRegStr HKLM "SOFTWARE\FM2i\PBSCC Proxy" "svn.work" ".svn"
	
	; delete deprecated values
	DeleteRegValue HKLM "SOFTWARE\FM2i\PBSCC Proxy" "noDDB"
	DeleteRegValue HKLM "SOFTWARE\FM2i\PBSCC Proxy" "last.ddb"
	DeleteRegValue HKLM "SOFTWARE\FM2i\PBSCC Proxy" "last.ver"
	
	WriteRegStr HKLM "SOFTWARE\SourceCodeControlProvider\InstalledSCCProviders" "PBSCC Proxy" "Software\FM2i\PBSCC Proxy"

	; Write the uninstall keys for Windows
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PBSCC Proxy" "DisplayName" "PBSCC Proxy"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PBSCC Proxy" "UninstallString" '"$INSTDIR\PbSccDel.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PBSCC Proxy" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PBSCC Proxy" "NoRepair" 1
	
	
	
	${EnvVarUpdate} $0 "PATH" "A" "HKLM" "$INSTDIR\bin"

SectionEnd ; end the section

;--------------------------------

; Uninstaller

Section "Uninstall"
  
	; Remove registry keys
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PBSCC Proxy"

	DeleteRegKey HKLM "SOFTWARE\FM2i\PBSCC Proxy"
	
	DeleteRegValue HKLM "SOFTWARE\SourceCodeControlProvider\InstalledSCCProviders" "PBSCC Proxy"
	
	; Remove files and uninstaller
	Delete $INSTDIR\Bin\pbscc.dll
	Delete "$INSTDIR\Bin\svnci.cmd"
	Delete "$INSTDIR\Bin\svnlog.cmd"
	Delete "$INSTDIR\Bin\svndiff.cmd"
	Delete $INSTDIR\PbSccDel.exe

SectionEnd
