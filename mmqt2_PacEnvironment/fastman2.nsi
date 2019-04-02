!include "MUI.nsh"
!include "LogicLib.nsh"

;SetCompressor lzma

!define APP_NAME "秒装助手"
!define COMPANY_NAME "杭州登先网络科技有限公司"
!define COMPANY_EMAIL "fay@dengxian.net"

Name "${APP_NAME}"
OutFile "${APP_NAME}_win.exe"
BrandingText "${COMPANY_NAME}"

InstallDir $PROGRAMFILES\${APP_NAME}

Function .onInit
ReadRegStr $R1 HKLM Software\FastmanV2 "Install_Dir"
${If} $R1 != ""
	StrCpy $INSTDIR $R1
${EndIf}
FunctionEnd

InstallDirRegKey HKLM "Software\FastmanV2" "Install_Dir"

RequestExecutionLevel admin

!define MUI_ICON "icon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange-uninstall.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\orange.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "${NSISDIR}\Contrib\Graphics\Header\orange-uninstall.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\fastman2\mmqt2.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "${APP_NAME}核心组件"
SectionIn RO
	setOutPath $INSTDIR
	File /r "fastman2"

	DetailPrint "正在安装 vcredist_x86..."
	ExecWait '"$INSTDIR\fastman2\vcredist_x86.exe" /q'
	Delete "$INSTDIR\fastman2\vcredist_x86.exe"

  ; Write the installation path into the registry
	WriteRegStr HKLM Software\FastmanV2 "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FastmanV2" "DisplayName" "${APP_NAME}卸载程序"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FastmanV2" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FastmanV2" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FastmanV2" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "开始菜单快捷方式"
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\fastman2\mmqt2.exe" "" "$INSTDIR\fastman2\mmqt2.exe"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\卸载.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

Section "桌面快捷方式"
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\fastman2\mmqt2.exe" "" "$INSTDIR\fastman2\mmqt2.exe"
SectionEnd

Section "Uninstall"
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\FastmanV2"
  DeleteRegKey HKLM SOFTWARE\FastmanV2

  Delete "$DESKTOP\${APP_NAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APP_NAME}"
  RMDir /r "$INSTDIR"
SectionEnd
