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

; ===== SauerEnhanced - RUNTIME FILES ONLY (NO SOURCE CODE) =====
; Game executable and required DLLs
Source: "SauerEnhanced\bin\*"; DestDir: "{app}\SauerEnhanced\bin"; Flags: ignoreversion recursesubdirs createallsubdirs

; Configuration files (root level)
Source: "SauerEnhanced\*.cfg"; DestDir: "{app}\SauerEnhanced"; Flags: ignoreversion
Source: "SauerEnhanced\*.txt"; DestDir: "{app}\SauerEnhanced"; Flags: ignoreversion
Source: "SauerEnhanced\*.mtl"; DestDir: "{app}\SauerEnhanced"; Flags: ignoreversion
Source: "SauerEnhanced\mapdata"; DestDir: "{app}\SauerEnhanced"; Flags: ignoreversion

; Config subdirectories
Source: "SauerEnhanced\cfg\*"; DestDir: "{app}\SauerEnhanced\cfg"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "SauerEnhanced\configs\*"; DestDir: "{app}\SauerEnhanced\configs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "SauerEnhanced\SauerEnhanced\*"; DestDir: "{app}\SauerEnhanced\SauerEnhanced"; Flags: ignoreversion recursesubdirs createallsubdirs

; Game data and assets
Source: "SauerEnhanced\data\*"; DestDir: "{app}\SauerEnhanced\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "SauerEnhanced\packages\*"; DestDir: "{app}\SauerEnhanced\packages"; Flags: ignoreversion recursesubdirs createallsubdirs

; User profiles (optional, but included for completeness)
Source: "SauerEnhanced\profiles\*"; DestDir: "{app}\SauerEnhanced\profiles"; Flags: ignoreversion recursesubdirs createallsubdirs

; NOTE: Explicitly EXCLUDED from installer:
; - SauerEnhanced\source\* (source code - not needed for playing)
; - SauerEnhanced\bin_unix\* (Unix binaries - not needed on Windows)
; - SauerEnhanced\*.pdb (debug symbols - not needed for release)
; - SauerEnhanced\*.md (developer documentation)
; - SauerEnhanced\compile_se (build script - not needed for playing)
; - SauerEnhanced\sauer_master (Unix binary)
; - SauerEnhanced\testmaps.zip (development test files)
; - SauerEnhanced\master.log (log files)
; ===== END SauerEnhanced FILES =====

; Data directory (root level game assets)
Source: "data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

; Packages directory (root level game assets)
Source: "packages\*"; DestDir: "{app}\packages"; Flags: ignoreversion recursesubdirs createallsubdirs

; Root files (documentation)
Source: "README.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "servers.cfg"; DestDir: "{app}"; Flags: ignoreversion; AfterInstall: CreateDesktopShortcut

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
// Function to remove previous installation before installing new version
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
  UninstallPath: String;
  UninstallExe: String;
begin
  Result := True; // Allow installation to continue by default
  
  // Check if a previous version is installed by looking for the uninstaller
  if RegQueryStringValue(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{AC2-CUBE2-GAME-2024}_is1', 
                         'UninstallString', UninstallPath) or
     RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{AC2-CUBE2-GAME-2024}_is1', 
                         'UninstallString', UninstallPath) then
  begin
    // Previous installation found
    UninstallPath := RemoveQuotes(UninstallPath);
    
    if FileExists(UninstallPath) then
    begin
      // Ask user if they want to uninstall the previous version
      if MsgBox('A previous installation of Adrenaline Cube 2 was detected.' + #13#10 + #13#10 + 
                'It is recommended to uninstall the previous version before continuing.' + #13#10 + 
                'Would you like to uninstall it now?' + #13#10 + #13#10 + 
                '(This will preserve your configuration files)', 
                mbConfirmation, MB_YESNO) = IDYES then
      begin
        // Run the uninstaller silently with /VERYSILENT /NORESTART /SUPPRESSMSGBOXES
        // The /VERYSILENT flag makes it completely silent
        if Exec(UninstallPath, '/VERYSILENT /NORESTART /SUPPRESSMSGBOXES', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
        begin
          // Wait a moment for files to be released
          Sleep(1000);
          
          // Uninstall successful
          if ResultCode = 0 then
          begin
            MsgBox('Previous installation removed successfully.' + #13#10 + 
                   'The new version will now be installed.', mbInformation, MB_OK);
          end
          else
          begin
            // Uninstall failed or was cancelled by user
            if MsgBox('The uninstaller returned an error code.' + #13#10 + 
                      'Do you want to continue with the installation anyway?', 
                      mbConfirmation, MB_YESNO) = IDNO then
            begin
              Result := False; // Cancel installation
            end;
          end;
        end
        else
        begin
          // Could not execute uninstaller
          if MsgBox('Could not run the uninstaller.' + #13#10 + 
                    'Do you want to continue with the installation anyway?', 
                    mbConfirmation, MB_YESNO) = IDNO then
          begin
            Result := False; // Cancel installation
          end;
        end;
      end;
    end;
  end;
end;

procedure CreateDesktopShortcut();
begin
  // This is called after the main files are installed
end;

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nAdrenaline Cube 2 is a fast-paced FPS game based on Cube 2: Sauerbraten with enhanced graphics and gameplay features.%n%nIt is recommended that you close all other applications before continuing.

