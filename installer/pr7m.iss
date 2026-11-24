; Paragon PR7-M installer (Inno Setup 6).
;   iscc /DAppVersion=0.1.0 installer\pr7m.iss      (after pyinstaller pr7m.spec has made dist\PR7-M)
;
; Per-user (no admin rights) into %LOCALAPPDATA%\Programs\PR7-M, with a Start menu entry; ticked by default,
; PR7-M registers as a media player: under "Open with" for video and music and in Settings > Default apps (PR7-M --register, so
; installer and Options share shell.py). Windows doesn't let a program make itself the default: Options opens
; Default apps for that one click. --unregister on uninstall. History, favorites and settings stay.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
; AppId is permanent: it's how a new version finds and upgrades the installed one. Never change it.
AppId={{CAA852D1-CDD0-4246-8ADB-50307D44F360}
AppName=Paragon PR7-M
AppVersion={#AppVersion}
AppVerName=Paragon PR7-M {#AppVersion}
AppPublisher=Paragon
VersionInfoVersion={#AppVersion}
PrivilegesRequired=lowest
DefaultDirName={autopf}\PR7-M
DefaultGroupName=Paragon PR7-M
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
UsePreviousTasks=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=PR7-M-Setup-{#AppVersion}
SetupIconFile=..\pr7m\icons\app.ico
UninstallDisplayIcon={app}\PR7-M.exe
UninstallDisplayName=Paragon PR7-M
WizardStyle=modern
WizardImageFile=wizard-large-1x.bmp,wizard-large-2x.bmp
WizardSmallImageFile=wizard-small-1x.bmp,wizard-small-2x.bmp
Compression=lzma2/max
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "mediaplayer"; Description: "Register PR7-M as a media player (Open with, Default apps)"; GroupDescription: "Windows:"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\PR7-M\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
Type: filesandordirs; Name: "{app}\_internal"

[Icons]
Name: "{autoprograms}\Paragon PR7-M"; Filename: "{app}\PR7-M.exe"; Comment: "Media player"
Name: "{autodesktop}\Paragon PR7-M"; Filename: "{app}\PR7-M.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PR7-M.exe"; Parameters: "--register"; Flags: runhidden waituntilterminated; Tasks: mediaplayer
Filename: "{app}\PR7-M.exe"; Parameters: "--unregister"; Flags: runhidden waituntilterminated; Tasks: not mediaplayer
Filename: "{app}\PR7-M.exe"; Description: "{cm:LaunchProgram,Paragon PR7-M}"; Flags: nowait postinstall skipifsilent
; Updates (Check for Updates runs this installer with /SILENT /relaunch=1): start PR7-M again when done.
Filename: "{app}\PR7-M.exe"; Flags: nowait runasoriginaluser; Check: Relaunch

[UninstallRun]
Filename: "{app}\PR7-M.exe"; Parameters: "--unregister"; Flags: runhidden waituntilterminated; RunOnceId: "Unregister"

[Code]
// True only when the updater asked for it (/relaunch=1): normal and CI installs behave as before.
function Relaunch: Boolean;
begin
  Result := ExpandConstant('{param:relaunch|0}') = '1';
end;
