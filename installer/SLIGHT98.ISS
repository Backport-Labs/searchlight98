; Searchlight 98 setup script.
; Build with Inno Setup 5.4.3 (the last release that supports Windows 95/98):
;   ISCC.exe installer\SLIGHT98.ISS
; or run build\build.ps1, which compiles SLIGHT98.EXE first.

#define AppName    "Searchlight 98"
#define AppVersion "0.3.4"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} version {#AppVersion}
AppPublisher=Backport Labs
AppCopyright=Copyright (C) 2026 Backport Labs
AppComments=A quick launcher for Windows 98
DefaultDirName={pf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
LicenseFile=..\LICENSE.TXT
OutputDir=..\dist
OutputBaseFilename=SL98SETUP
SetupIconFile=..\app\SLIGHT98.ICO
UninstallDisplayIcon={app}\SLIGHT98.ICO
UninstallDisplayName={#AppName}
WizardImageFile=WIZARD.BMP
WizardSmallImageFile=WIZSMALL.BMP
; A full-screen gradient behind the wizard.
WindowVisible=yes
WindowShowCaption=yes
BackColor=$800000
BackColor2=$000000
BackColorDirection=toptobottom
Compression=lzma
SolidCompression=yes
MinVersion=4.0,4.0
PrivilegesRequired=none

[Tasks]
Name: "startup"; Description: "Start {#AppName} automatically when Windows starts (recommended)"; GroupDescription: "Options:"
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Options:"; Flags: unchecked

[Dirs]
Name: "{app}\EXTRA"

[Files]
Source: "..\build\out\SLIGHT98.EXE"; DestDir: "{app}"; Flags: ignoreversion
; Used for shortcut icons only. The program carries its own icon.
Source: "..\app\SLIGHT98.ICO"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\app\README.TXT"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\LICENSE.TXT"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\app\FILE_ID.DIZ"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\app\EXTRA\README.TXT"; DestDir: "{app}\EXTRA"; Flags: ignoreversion
; Keep the user's own groups when upgrading.
Source: "..\app\CATEGORY.TXT"; DestDir: "{app}"; Flags: onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\SLIGHT98.EXE"; IconFilename: "{app}\SLIGHT98.ICO"; Comment: "Find and start any program (Ctrl+Space)"
Name: "{group}\Read Me"; Filename: "{app}\README.TXT"
Name: "{group}\Groups and hidden programs (CATEGORY.TXT)"; Filename: "{app}\CATEGORY.TXT"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#AppName}"; Filename: "{app}\SLIGHT98.EXE"; IconFilename: "{app}\SLIGHT98.ICO"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\SLIGHT98.EXE"""; Tasks: startup; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Searchlight 98"; Flags: uninsdeletekey

[Run]
Filename: "{app}\SLIGHT98.EXE"; Description: "Start {#AppName} now"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{app}\SLIGHT98.EXE"; Parameters: "/uninstall"; Flags: runhidden

[UninstallDelete]
Type: files; Name: "{app}\STATE.TXT"
Type: files; Name: "{app}\SLIGHT98.LOG"

[Code]
// Stop a running copy before files are replaced, so an upgrade needs no restart.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Code: Integer;
begin
  if FileExists(ExpandConstant('{app}\SLIGHT98.EXE')) then
  begin
    Exec(ExpandConstant('{app}\SLIGHT98.EXE'), '/quit', '', SW_HIDE, ewWaitUntilTerminated, Code);
    Sleep(1500);
  end;
  Result := '';
end;
