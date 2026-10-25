; Hearizon Sync C.R.V. 安装程序
Unicode true

!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef APP_VERSION
  !define APP_VERSION "2.5.6"
!endif
!define APP_NAME "Hearizon Sync C.R.V."
!define APP_NAME_EN "HearizonSyncCRV"
!define APP_PUBLISHER "何瑞程"
!define APP_EXE "HearizonSync.exe"
!define APP_REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_EN}"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "HearizonSync-CRV-Setup-${APP_VERSION}.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${APP_REGKEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 64

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName"     "${APP_NAME}"
VIAddVersionKey "ProductVersion"  "${APP_VERSION}"
VIAddVersionKey "CompanyName"     "${APP_PUBLISHER}"
VIAddVersionKey "FileDescription" "${APP_NAME} 安装程序"
VIAddVersionKey "FileVersion"     "${APP_VERSION}.0"
VIAddVersionKey "LegalCopyright"  "© 2026 ${APP_PUBLISHER}"

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Section "Main" SecMain
  SetOutPath "$INSTDIR"

  ; 主程序与运行库
  File "..\build\HearizonSync.exe"
  File "..\build\onnxruntime.dll"
  File "..\build\sherpa-onnx-c-api.dll"
  File "..\build\ctranslate2.dll"
  File "..\build\libiomp5md.dll"

  ; 图标
  File "..\build\app.ico"
  File "..\build\app-icon.ico"
  File "..\build\app-icon-live.ico"

  ; 模型
  SetOutPath "$INSTDIR\models\sv"
  File "..\models\sv\model.int8.onnx"
  File "..\models\sv\tokens.txt"
  SetOutPath "$INSTDIR\models\opus_en_zh"
  File "..\models\opus_en_zh\model.bin"
  File "..\models\opus_en_zh\source.spm"
  File "..\models\opus_en_zh\target.spm"
  File "..\models\opus_en_zh\shared_vocabulary.json"
  File "..\models\opus_en_zh\vocab.json"
  File "..\models\opus_en_zh\config.json"

  SetOutPath "$INSTDIR"

  ; 快捷方式
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}\卸载 ${APP_NAME}.lnk" "$INSTDIR\uninstall.exe"
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  ; 卸载信息
  WriteRegStr HKLM "${APP_REGKEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKLM "${APP_REGKEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${APP_REGKEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "${APP_REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${APP_REGKEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKLM "${APP_REGKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${APP_REGKEY}" "NoRepair" 1

  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\onnxruntime.dll"
  Delete "$INSTDIR\sherpa-onnx-c-api.dll"
  Delete "$INSTDIR\ctranslate2.dll"
  Delete "$INSTDIR\libiomp5md.dll"
  Delete "$INSTDIR\app.ico"
  Delete "$INSTDIR\app-icon.ico"
  Delete "$INSTDIR\app-icon-live.ico"
  Delete "$INSTDIR\uninstall.exe"
  RMDir /r "$INSTDIR\models"
  RMDir /r "$INSTDIR"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\卸载 ${APP_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  DeleteRegKey HKLM "${APP_REGKEY}"
SectionEnd
