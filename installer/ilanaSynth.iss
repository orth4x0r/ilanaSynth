#define AppName "ilanaSynth"
#define AppVersion "1.1"
#define AppPublisher "Ilana Audio"
#define RepoRoot ".."

[Setup]
AppId={{B7E4A9C2-5D1F-4E8A-9C3B-2F6D1A7E4C58}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} v{#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
OutputDir={#RepoRoot}\release
OutputBaseFilename=ilanaSynth-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\ilanaSynth.exe
VersionInfoVersion=1.1.0.0
VersionInfoProductName={#AppName}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=ilanaSynth v{#AppVersion} installer

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Files]
Source: "{#RepoRoot}\build\ilanaSynth_artefacts\Release\VST3\ilanaSynth.vst3\*"; DestDir: "{commoncf}\VST3\ilanaSynth.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\build\ilanaSynth_artefacts\Release\Standalone\ilanaSynth.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\ilanaSynth.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\ilanaSynth.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ilanaSynth.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
