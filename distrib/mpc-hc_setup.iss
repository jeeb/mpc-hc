; (C) 2009-2012 see Authors.txt
;
; This file is part of MPC-HC.
;
; MPC-HC is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 3 of the License, or
; (at your option) any later version.
;
; MPC-HC is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
; $Id$


; Requirements:
; Inno Setup Unicode: http://www.jrsoftware.org/isdl.php


; If you want to compile the 64-bit version define "x64build" (uncomment the define below or use build_2010.bat)
#define localize
#define sse_required
;#define x64Build

; Don't forget to update the DirectX SDK number in include\Version.h (not updated so often)


; From now on you shouldn't need to change anything

#if VER < EncodeVer(5,4,3)
  #error Update your Inno Setup version (5.4.3 or newer)
#endif

#ifndef UNICODE
  #error Use the Unicode Inno Setup
#endif


#define ISPP_IS_BUGGY
#include "..\include\Version.h"

#define copyright_year "2002-2012"
#define app_name       "Media Player Classic - Home Cinema"
#define app_version    str(MPC_VERSION_MAJOR) + "." + str(MPC_VERSION_MINOR) + "." + str(MPC_VERSION_PATCH) + "." + str(MPC_VERSION_REV)
#define quick_launch   "{userappdata}\Microsoft\Internet Explorer\Quick Launch"


#ifdef x64Build
  #define bindir       = "..\bin\mpc-hc_x64"
  #define mpchc_exe    = "mpc-hc64.exe"
  #define mpchc_ini    = "mpc-hc64.ini"
#else
  #define bindir       = "..\bin\mpc-hc_x86"
  #define mpchc_exe    = "mpc-hc.exe"
  #define mpchc_ini    = "mpc-hc.ini"
#endif


[Setup]
#ifdef x64Build
AppId={{2ACBF1FA-F5C3-4B19-A774-B22A31F231B9}
DefaultGroupName={#app_name} x64
OutputBaseFilename=MPC-HomeCinema.{#app_version}.x64
UninstallDisplayName={#app_name} {#app_version} x64
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
#else
AppId={{2624B969-7135-4EB1-B0F6-2D8C397B45F7}
DefaultGroupName={#app_name}
OutputBaseFilename=MPC-HomeCinema.{#app_version}.x86
UninstallDisplayName={#app_name} {#app_version}
#endif

AppName={#app_name}
AppVersion={#app_version}
AppVerName={#app_name} {#app_version}
AppPublisher=MPC-HC Team
AppPublisherURL=http://mpc-hc.sourceforge.net/
AppSupportURL=http://mpc-hc.sourceforge.net/
AppUpdatesURL=http://mpc-hc.sourceforge.net/
AppContact=http://mpc-hc.sourceforge.net/
AppCopyright=Copyright � {#copyright_year} all contributors, see Authors.txt
VersionInfoCompany=MPC-HC Team
VersionInfoCopyright=Copyright � {#copyright_year}, MPC-HC Team
VersionInfoDescription={#app_name} Setup
VersionInfoProductName={#app_name}
VersionInfoProductVersion={#app_version}
VersionInfoProductTextVersion={#app_version}
VersionInfoTextVersion={#app_version}
VersionInfoVersion={#app_version}
UninstallDisplayIcon={app}\{#mpchc_exe}
DefaultDirName={code:GetInstallFolder}
LicenseFile=..\COPYING.txt
OutputDir=.
SetupIconFile=..\src\apps\mplayerc\res\icon.ico
AppReadmeFile={app}\Readme.txt
WizardImageFile=WizardImageFile.bmp
WizardSmallImageFile=WizardSmallImageFile.bmp
Compression=lzma/ultra
SolidCompression=yes
AllowNoIcons=yes
ShowTasksTreeLines=yes
DisableDirPage=auto
DisableProgramGroupPage=auto
MinVersion=0,5.01.2600sp3
AppMutex=MediaPlayerClassicW


[Languages]
Name: en; MessagesFile: compiler:Default.isl

#ifdef localize
Name: br; MessagesFile: compiler:Languages\BrazilianPortuguese.isl
Name: by; MessagesFile: Languages\Belarusian.isl
Name: ca; MessagesFile: compiler:Languages\Catalan.isl
Name: cz; MessagesFile: compiler:Languages\Czech.isl
Name: de; MessagesFile: compiler:Languages\German.isl
Name: es; MessagesFile: compiler:Languages\Spanish.isl
Name: fr; MessagesFile: compiler:Languages\French.isl
Name: he; MessagesFile: compiler:Languages\Hebrew.isl
Name: hu; MessagesFile: Languages\Hungarian.isl
Name: hy; MessagesFile: Languages\Armenian.islu
Name: it; MessagesFile: compiler:Languages\Italian.isl
Name: ja; MessagesFile: compiler:Languages\Japanese.isl
Name: kr; MessagesFile: Languages\Korean.isl
Name: nl; MessagesFile: compiler:Languages\Dutch.isl
Name: pl; MessagesFile: compiler:Languages\Polish.isl
Name: ru; MessagesFile: compiler:Languages\Russian.isl
Name: sc; MessagesFile: Languages\ChineseSimp.isl
Name: se; MessagesFile: Languages\Swedish.isl
Name: sk; MessagesFile: compiler:Languages\Slovak.isl
Name: tc; MessagesFile: Languages\ChineseTrad.isl
Name: tr; MessagesFile: Languages\Turkish.isl
Name: ua; MessagesFile: Languages\Ukrainian.isl
#endif

; Include installer's custom messages
#include "custom_messages.iss"


[Messages]
BeveledLabel={#app_name} {#app_version}


[Types]
Name: default;            Description: {cm:types_DefaultInstallation}
Name: custom;             Description: {cm:types_CustomInstallation};                     Flags: iscustom


[Components]
Name: main;               Description: {#app_name} {#app_version}; Types: default custom; Flags: fixed
Name: mpciconlib;         Description: {cm:comp_mpciconlib};       Types: default custom
#ifdef localize
Name: mpcresources;       Description: {cm:comp_mpcresources};     Types: default custom; Flags: disablenouninstallwarning
#endif


[Tasks]
Name: desktopicon;        Description: {cm:CreateDesktopIcon};     GroupDescription: {cm:AdditionalIcons}
Name: desktopicon\user;   Description: {cm:tsk_CurrentUser};       GroupDescription: {cm:AdditionalIcons}; Flags: exclusive
Name: desktopicon\common; Description: {cm:tsk_AllUsers};          GroupDescription: {cm:AdditionalIcons}; Flags: unchecked exclusive
Name: quicklaunchicon;    Description: {cm:CreateQuickLaunchIcon}; GroupDescription: {cm:AdditionalIcons}; Flags: unchecked;             OnlyBelowVersion: 0,6.01
Name: reset_settings;     Description: {cm:tsk_ResetSettings};     GroupDescription: {cm:tsk_Other};       Flags: checkedonce unchecked; Check: SettingsExistCheck()


[Files]
Source: {#bindir}\{#mpchc_exe};             DestDir: {app};      Components: main;         Flags: ignoreversion
Source: {#bindir}\mpciconlib.dll;           DestDir: {app};      Components: mpciconlib;   Flags: ignoreversion
#ifdef localize
Source: {#bindir}\Lang\mpcresources.br.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.by.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.ca.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.cz.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.de.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.es.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.fr.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.he.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.hu.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.hy.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.it.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.ja.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.kr.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.nl.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.pl.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.ru.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.sc.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.sk.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.sv.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.tc.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.tr.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
Source: {#bindir}\Lang\mpcresources.ua.dll; DestDir: {app}\Lang; Components: mpcresources; Flags: ignoreversion
#endif
Source: ..\docs\Authors.txt;                DestDir: {app};      Components: main;         Flags: ignoreversion
Source: ..\docs\Changelog.txt;              DestDir: {app};      Components: main;         Flags: ignoreversion
Source: ..\COPYING.txt;                     DestDir: {app};      Components: main;         Flags: ignoreversion
Source: ..\docs\Readme.txt;                 DestDir: {app};      Components: main;         Flags: ignoreversion


[Icons]
#ifdef x64Build
Name: {group}\{#app_name} x64;                   Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version} x64; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0
Name: {commondesktop}\{#app_name} x64;           Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version} x64; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\common
Name: {userdesktop}\{#app_name} x64;             Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version} x64; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\user
Name: {#quick_launch}\{#app_name} x64;           Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version} x64; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: quicklaunchicon
#else
Name: {group}\{#app_name};                       Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0
Name: {commondesktop}\{#app_name};               Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\common
Name: {userdesktop}\{#app_name};                 Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: desktopicon\user
Name: {#quick_launch}\{#app_name};               Filename: {app}\{#mpchc_exe}; Comment: {#app_name} {#app_version}; WorkingDir: {app}; IconFilename: {app}\{#mpchc_exe}; IconIndex: 0; Tasks: quicklaunchicon
#endif
Name: {group}\Changelog;                         Filename: {app}\Changelog.txt; Comment: {cm:ViewChangelog};                WorkingDir: {app}
Name: {group}\{cm:ProgramOnTheWeb,{#app_name}};  Filename: http://mpc-hc.sourceforge.net/
Name: {group}\{cm:UninstallProgram,{#app_name}}; Filename: {uninstallexe};      Comment: {cm:UninstallProgram,{#app_name}}; WorkingDir: {app}


[Run]
Filename: {app}\{#mpchc_exe};                    Description: {cm:LaunchProgram,{#app_name}}; WorkingDir: {app}; Flags: nowait postinstall skipifsilent unchecked
Filename: {app}\Changelog.txt;                   Description: {cm:ViewChangelog};             WorkingDir: {app}; Flags: nowait postinstall skipifsilent unchecked shellexec


[InstallDelete]
Type: files; Name: {userdesktop}\{#app_name}.lnk;   Check: not IsTaskSelected('desktopicon\user')   and IsUpgrade()
Type: files; Name: {commondesktop}\{#app_name}.lnk; Check: not IsTaskSelected('desktopicon\common') and IsUpgrade()
Type: files; Name: {#quick_launch}\{#app_name}.lnk; Check: not IsTaskSelected('quicklaunchicon')    and IsUpgrade(); OnlyBelowVersion: 0,6.01
Type: files; Name: {app}\AUTHORS
Type: files; Name: {app}\ChangeLog
Type: files; Name: {app}\COPYING
#ifdef localize
; remove the old language dlls when upgrading
Type: files; Name: {app}\mpcresources.br.dll
Type: files; Name: {app}\mpcresources.by.dll
Type: files; Name: {app}\mpcresources.ca.dll
Type: files; Name: {app}\mpcresources.cz.dll
Type: files; Name: {app}\mpcresources.de.dll
Type: files; Name: {app}\mpcresources.es.dll
Type: files; Name: {app}\mpcresources.fr.dll
Type: files; Name: {app}\mpcresources.he.dll
Type: files; Name: {app}\mpcresources.hu.dll
Type: files; Name: {app}\mpcresources.hy.dll
Type: files; Name: {app}\mpcresources.it.dll
Type: files; Name: {app}\mpcresources.ja.dll
Type: files; Name: {app}\mpcresources.kr.dll
Type: files; Name: {app}\mpcresources.nl.dll
Type: files; Name: {app}\mpcresources.pl.dll
Type: files; Name: {app}\mpcresources.ru.dll
Type: files; Name: {app}\mpcresources.sc.dll
Type: files; Name: {app}\mpcresources.sk.dll
Type: files; Name: {app}\mpcresources.sv.dll
Type: files; Name: {app}\mpcresources.tc.dll
Type: files; Name: {app}\mpcresources.tr.dll
Type: files; Name: {app}\mpcresources.ua.dll
#endif


[Code]
#if defined(sse_required) || defined(sse2_required)
function IsProcessorFeaturePresent(Feature: Integer): Boolean;
external 'IsProcessorFeaturePresent@kernel32.dll stdcall';
#endif

const installer_mutex_name = 'mpchc_setup_mutex';


function GetInstallFolder(Default: String): String;
var
  sInstallPath: String;
begin
  if not RegQueryStringValue(HKLM, 'SOFTWARE\Gabest\Media Player Classic', 'ExePath', sInstallPath) then begin
    Result := ExpandConstant('{pf}\Media Player Classic - Home Cinema');
  end
  else begin
    RegQueryStringValue(HKLM, 'SOFTWARE\Gabest\Media Player Classic', 'ExePath', sInstallPath)
    Result := ExtractFileDir(sInstallPath);
    if (Result = '') or not DirExists(Result) then begin
      Result := ExpandConstant('{pf}\Media Player Classic - Home Cinema');
    end;
  end;
end;


function D3DX9DLLExists(): Boolean;
begin
  if FileExists(ExpandConstant('{sys}\D3DX9_{#DIRECTX_SDK_NUMBER}.dll')) then
    Result := True
  else
    Result := False;
end;


#if defined(sse_required)
function Is_SSE_Supported(): Boolean;
begin
  // PF_XMMI_INSTRUCTIONS_AVAILABLE
  Result := IsProcessorFeaturePresent(6);
end;

#elif defined(sse2_required)

function Is_SSE2_Supported(): Boolean;
begin
  // PF_XMMI64_INSTRUCTIONS_AVAILABLE
  Result := IsProcessorFeaturePresent(10);
end;

#endif


function IsUpgrade(): Boolean;
var
  sPrevPath: String;
begin
  sPrevPath := WizardForm.PrevAppDir;
  Result := (sPrevPath <> '');
end;


// Check if MPC-HC's settings exist
function SettingsExistCheck(): Boolean;
begin
  if RegKeyExists(HKEY_CURRENT_USER, 'Software\Gabest\Media Player Classic') or
  FileExists(ExpandConstant('{app}\{#mpchc_ini}')) then
    Result := True
  else
    Result := False;
end;


function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // Hide the License page
  if IsUpgrade() and (PageID = wpLicense) then
    Result := True;
end;


procedure CleanUpSettingsAndFiles();
begin
  DeleteFile(ExpandConstant('{app}\*.bak'));
  DeleteFile(ExpandConstant('{app}\{#mpchc_ini}'));
  DeleteFile(ExpandConstant('{userappdata}\Media Player Classic\default.mpcpl'));
  RemoveDir(ExpandConstant('{userappdata}\Media Player Classic'));
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Gabest\Filters');
  RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Gabest\Media Player Classic');
  RegDeleteKeyIncludingSubkeys(HKLM, 'SOFTWARE\Gabest\Media Player Classic');
  RegDeleteKeyIfEmpty(HKCU, 'Software\Gabest');
  RegDeleteKeyIfEmpty(HKLM, 'SOFTWARE\Gabest');
end;


procedure CurStepChanged(CurStep: TSetupStep);
var
  iLanguage: Integer;
begin
  if CurStep = ssPostInstall then begin
    if IsTaskSelected('reset_settings') then
      CleanUpSettingsAndFiles();

    iLanguage := StrToInt(ExpandConstant('{cm:langid}'));
    RegWriteStringValue(HKLM, 'SOFTWARE\Gabest\Media Player Classic', 'ExePath', ExpandConstant('{app}\{#mpchc_exe}'));

    if IsComponentSelected('mpcresources') and FileExists(ExpandConstant('{app}\{#mpchc_ini}')) then
      SetIniInt('Settings', 'InterfaceLanguage', iLanguage, ExpandConstant('{app}\{#mpchc_ini}'))
    else
      RegWriteDWordValue(HKCU, 'Software\Gabest\Media Player Classic\Settings', 'InterfaceLanguage', iLanguage);
  end;

  if (CurStep = ssDone) and not WizardSilent() and not D3DX9DLLExists() then
    SuppressibleMsgBox(CustomMessage('msg_NoD3DX9DLL_found'), mbError, MB_OK, MB_OK);

end;


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  // When uninstalling, ask the user to delete MPC-HC settings
  if (CurUninstallStep = usUninstall) and SettingsExistCheck() then begin
    if SuppressibleMsgBox(CustomMessage('msg_DeleteSettings'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDYES then
      CleanUpSettingsAndFiles();
  end;
end;


function InitializeSetup(): Boolean;
begin
  // Create a mutex for the installer and if it's already running display a message and stop installation
  if CheckForMutexes(installer_mutex_name) and not WizardSilent() then begin
    SuppressibleMsgBox(CustomMessage('msg_SetupIsRunningWarning'), mbError, MB_OK, MB_OK);
    Result := False;
  end
  else begin
    Result := True;
    CreateMutex(installer_mutex_name);

#if defined(sse2_required)
    if not Is_SSE2_Supported() then begin
      SuppressibleMsgBox(CustomMessage('msg_simd_sse2'), mbCriticalError, MB_OK, MB_OK);
      Result := False;
    end;
#elif defined(sse_required)
    if not Is_SSE_Supported() then begin
      SuppressibleMsgBox(CustomMessage('msg_simd_sse'), mbCriticalError, MB_OK, MB_OK);
      Result := False;
    end;
#endif

  end;
end;
