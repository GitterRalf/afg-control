; Build AFG-Control.exe and deploy its Qt runtime with build.ps1 before compiling.
; The application loads NI-488.2 from Windows. Install the NI driver separately.
#define ReadQuoted(Text) Copy(Text, Pos('"', Text) + 1, Pos('"', Copy(Text, Pos('"', Text) + 1, Len(Text))) - 1)
#define VersionHeader FileOpen(SourcePath + "version.h")
#expr FileRead(VersionHeader)
#expr FileRead(VersionHeader)
#define AfgVersion ReadQuoted(FileRead(VersionHeader))
#expr FileClose(VersionHeader)
#define IconResource FileOpen(SourcePath + "assets\app.rc")
#define AfgIcon ReadQuoted(FileRead(IconResource))
#expr FileClose(IconResource)
#define AfgName "AFG Control"
#define AfgExe "AFG-Control.exe"

[Setup]
AppId={{5A3A7304-A491-4D92-A564-E4080EE52973}
AppName={#AfgName}
AppVersion={#AfgVersion}
AppVerName={#AfgName} {#AfgVersion}
AppPublisher=Ralf Tralow
VersionInfoVersion={#AfgVersion}
VersionInfoDescription={#AfgName} Setup
DefaultDirName={pf}\{#AfgName}
DefaultGroupName={#AfgName}
AllowNoIcons=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; Inno 5.3.5 can see modern Windows as 6.2 due to its legacy manifest.
SetupIconFile=assets\{#AfgIcon}
UninstallDisplayIcon={app}\{#AfgExe}
OutputDir=installer
OutputBaseFilename=AFG-Control-Setup-{#AfgVersion}-x64
Compression=lzma
SolidCompression=yes

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "build\{#AfgExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\generic\*.dll"; DestDir: "{app}\generic"; Flags: ignoreversion
Source: "build\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion
Source: "build\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "build\networkinformation\*.dll"; DestDir: "{app}\networkinformation"; Flags: ignoreversion
Source: "build\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "build\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "build\tls\*.dll"; DestDir: "{app}\tls"; Flags: ignoreversion
Source: "LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "licenses\*.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AfgName}"; Filename: "{app}\{#AfgExe}"; WorkingDir: "{app}"
Name: "{commondesktop}\{#AfgName}"; Filename: "{app}\{#AfgExe}"; WorkingDir: "{app}"; Tasks: desktopicon
