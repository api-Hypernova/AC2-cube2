; Adrenaline Cube 2 (AC2) - Inno Setup Installer Script
; This script creates a Windows installer for AC2

#define MyAppName "Adrenaline Cube 2"
#define MyAppVersion "0.1 Beta"
#define MyAppPublisher "»Ąрī« and Contributors"
#define MyAppURL "https://github.com/yourusername/AC2-cube2"
#define MyAppExeName "SauerEnhanced.bat"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
AppId={{AC2-CUBE2-GAME-2024}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
; LicenseFile=data\license.txt  (Disabled - no license acceptance required)
OutputDir=installer_output
OutputBaseFilename=AC2_Setup
; SetupIconFile=data\cube2badge.png (PNG not supported - convert to .ico if desired)
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\data\cube2badge.png
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable launcher
Source: "SauerEnhanced.bat"; DestDir: "{app}"; Flags: ignoreversion

; SauerEnhanced directory and all contents
Source: "SauerEnhanced\*"; DestDir: "{app}\SauerEnhanced"; Flags: ignoreversion recursesubdirs createallsubdirs

; Data directory
Source: "data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

; Packages directory
Source: "packages\*"; DestDir: "{app}\packages"; Flags: ignoreversion recursesubdirs createallsubdirs

; Docs directory
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

; Root files
Source: "README.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "servers.cfg"; DestDir: "{app}"; Flags: ignoreversion; AfterInstall: CreateDesktopShortcut

; Linux files (optional, for reference)
Source: "SauerEnhanced_linux.sh"; DestDir: "{app}"; Flags: ignoreversion
Source: "sauerbraten_unix"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Start Menu shortcut
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\data\cube2badge.png"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{group}\README"; Filename: "{app}\README.html"

; Desktop shortcut (if user selected it)
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\data\cube2badge.png"; Tasks: desktopicon

[Run]
; Option to launch game after installation
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent shellexec

[Code]
procedure CreateDesktopShortcut();
begin
  // This is called after the main files are installed
end;

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nAdrenaline Cube 2 is a fast-paced FPS game based on Cube 2: Sauerbraten with enhanced graphics and gameplay features.%n%nIt is recommended that you close all other applications before continuing.

