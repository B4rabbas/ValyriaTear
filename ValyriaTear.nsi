; Script originally generated by the HM NIS Edit Script Wizard.

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "Valyria Tear"
!define PRODUCT_VERSION "Episode I"
!define PRODUCT_PUBLISHER "Bertram"
!define PRODUCT_WEB_SITE "https://github.com/Bertram25/ValyriaTear/wiki"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\ValyriaTear.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"


!define LICENSE_FILE "license-file"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "data/icons/program_icon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page (not needed)
; !insertmacro MUI_PAGE_LICENSE $LICENSE_FILE
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\$0.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "valyriatear-win32-installer.exe"
InstallDir "$PROGRAMFILES\ValyriaTear"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
  StrCpy $0 "ValyriaTear"
  SetOverwrite ifnewer

  SetOutPath "$INSTDIR\data"
  File /r data\*

; The offline manual
  SetOutPath "$INSTDIR\doc"
  File /r doc\*

;  TODO: Add proper support for translatable languages
;  SetOutPath "$INSTDIR\po"
;  File /r po\*
; For now, we copy files by hand
  SetOutPath "$INSTDIR\po\fr\LC_MESSAGES"
  File /oname=valyriatear.mo po\fr.mo
  SetOutPath "$INSTDIR\po\it\LC_MESSAGES"
  File /oname=valyriatear.mo po\it.mo
  SetOutPath "$INSTDIR\po\de\LC_MESSAGES"
  File /oname=valyriatear.mo po\de.mo
  SetOutPath "$INSTDIR\po\es\LC_MESSAGES"
  File /oname=valyriatear.mo po\es.mo
  SetOutPath "$INSTDIR\po\gl\LC_MESSAGES"
  File /oname=valyriatear.mo po\gl.mo
  SetOutPath "$INSTDIR\po\gd\LC_MESSAGES"
  File /oname=valyriatear.mo po\gd.mo
  SetOutPath "$INSTDIR\po\nl_NL\LC_MESSAGES"
  File /oname=valyriatear.mo po\nl_NL.mo
  SetOutPath "$INSTDIR\po\pt_BR\LC_MESSAGES"
  File /oname=valyriatear.mo po\pt_BR.mo
  SetOutPath "$INSTDIR\po\pt_PT\LC_MESSAGES"
  File /oname=valyriatear.mo po\pt_PT.mo
  SetOutPath "$INSTDIR\po\ru\LC_MESSAGES"
  File /oname=valyriatear.mo po\ru.mo
  SetOutPath "$INSTDIR\po\en_GB\LC_MESSAGES"
  File /oname=valyriatear.mo po\en_GB.mo
  SetOutPath "$INSTDIR\po\sv\LC_MESSAGES"
  File /oname=valyriatear.mo po\sv.mo

  SetOutPath "$INSTDIR"
  File valyriaTear.exe
  File *.dll

  CreateDirectory "$SMPROGRAMS\ValyriaTear"
  CreateShortCut "$SMPROGRAMS\ValyriaTear\$0.lnk" "$INSTDIR\$0.exe"
  CreateShortCut "$SMPROGRAMS\ValyriaTear\Manual.lnk" "$INSTDIR\doc\index.htm"
SectionEnd

Section -AdditionalIcons
  CreateShortCut "$SMPROGRAMS\ValyriaTear\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\${PRODUCT_NAME}.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\${PRODUCT_NAME}.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd


Function un.onUninstSuccess
;  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  StrCpy $0 "ValyriaTear"

  Delete "$SMPROGRAMS\ValyriaTear\Uninstall.lnk"
  Delete "$SMPROGRAMS\ValyriaTear\$0.lnk"
  Delete "$SMPROGRAMS\ValyriaTear\Manual.lnk"

  RMDir "$SMPROGRAMS\ValyriaTear"
  RMDir /r "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
