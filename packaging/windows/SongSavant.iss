; Song Savant - Windows installer (Inno Setup 6)
;
;   iscc /DAppVersion=0.1.0 /DArtefacts="C:\path\to\build\SongSavant_artefacts\Release"
;        /DScanner="C:\path\to\build\SongSavantScanner_artefacts\Release\Song Savant Scanner.exe" packaging\windows\SongSavant.iss
;
; Installs:
;   VST3        -> C:\Program Files\Common Files\VST3\Song Savant.vst3
;   Standalone  -> C:\Program Files\Song Savant\Song Savant.exe  (+ Start menu / desktop shortcut)
;   Scanner     -> C:\Program Files\Song Savant\Song Savant Scanner.exe  (always: the app and the plug-in both use it
;                  to find out what a plug-in is; the plug-in finds it through the registry value below)

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\..\build\SongSavant_artefacts\Release"
#endif
#ifndef Scanner
  #define Scanner "..\..\build\SongSavantScanner_artefacts\Release\Song Savant Scanner.exe"
#endif

[Setup]
AppId={{91EF4266-1C65-42CC-89CF-720F9F7F4A47}
AppName=Song Savant
AppVersion={#AppVersion}
AppPublisher=Grabber Audio
DefaultDirName={autopf}\Song Savant
DefaultGroupName=Song Savant
DisableProgramGroupPage=yes
OutputBaseFilename=Song-Savant-{#AppVersion}-Windows-Setup
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\Song Savant.exe
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Types]
Name: "full";   Description: "Standalone app + plug-in"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "app";  Description: "Standalone app"; Types: full custom
Name: "vst3"; Description: "VST3 plug-in (Cubase, Cakewalk, FL Studio, Ableton Live, Studio One, Reaper, Bitwig ...)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: app

[Files]
Source: "{#Artefacts}\Standalone\Song Savant.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#Artefacts}\VST3\Song Savant.vst3\*"; DestDir: "{commoncf64}\VST3\Song Savant.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Scanner}"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
Root: HKLM; Subkey: "Software\Grabber Audio\Song Savant"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey

[Icons]
Name: "{group}\Song Savant"; Filename: "{app}\Song Savant.exe"; Components: app
Name: "{autodesktop}\Song Savant"; Filename: "{app}\Song Savant.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Song Savant.exe"; Description: "Open Song Savant"; Flags: nowait postinstall skipifsilent runasoriginaluser; Components: app

[Messages]
FinishedLabel=Song Savant is installed.%n%nOpen it, describe your song on the SONG page and click FIND MY SOUNDS. The LIBRARY page lists every sound it found.
