#define MyAppName      "MyMapMap"
#define MyAppVersion   "1.0.1"
#define MyAppPublisher "Kulp Lights"
#define MyAppURL       "https://github.com/derwin12/MyMapMap"
#define MyAppExeName   "mymapmap.exe"
#define MyAppAssocExt  ".mmp"
#define MyAppAssocKey  "MyMapMap.Project"
#define BinDir         "..\bin"

[Setup]
AppId={{A3F2C1D4-8B7E-4F9A-BC3D-2E6F1A0D5C8B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir=..\installer\output
OutputBaseFilename=MyMapMap-{#MyAppVersion}-Setup
SetupIconFile=..\resources\app_icons\mymapmap.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";    Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "assocmmp";       Description: "Associate .mmp project files with {#MyAppName}"; GroupDescription: "File associations:"; Flags: checkedonce

[Files]
Source: "{#BinDir}\{#MyAppExeName}";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#BinDir}\*.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#BinDir}\generic\*";        DestDir: "{app}\generic";        Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\iconengines\*";    DestDir: "{app}\iconengines";    Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\imageformats\*";   DestDir: "{app}\imageformats";   Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\multimedia\*";     DestDir: "{app}\multimedia";     Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\platforms\*";      DestDir: "{app}\platforms";      Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\styles\*";         DestDir: "{app}\styles";         Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\tls\*";            DestDir: "{app}\tls";            Flags: ignoreversion recursesubdirs
Source: "{#BinDir}\translations\*";   DestDir: "{app}\translations";   Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}";                 Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";           Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; .mmp file association
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocExt}";                            ValueType: string; ValueName: ""; ValueData: "{#MyAppAssocKey}";  Flags: uninsdeletevalue;  Tasks: assocmmp
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocKey}";                            ValueType: string; ValueName: ""; ValueData: "MyMapMap Project"; Flags: uninsdeletekey;   Tasks: assocmmp
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocKey}\DefaultIcon";                ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0";                  Tasks: assocmmp
Root: HKA; Subkey: "Software\Classes\{#MyAppAssocKey}\shell\open\command";         ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1""";         Tasks: assocmmp
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".mmp"; ValueData: "";                                     Tasks: assocmmp

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
