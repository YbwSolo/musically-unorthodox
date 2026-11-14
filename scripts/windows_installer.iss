; Musically Unorthodox for Windows: the VST3 plugin and the standalone app.
#ifndef AppVersion
  #define AppVersion "0.5.0"
#endif
#ifndef Artefacts
  #define Artefacts "..\build\MusicallyUnorthodox_artefacts\Release"
#endif

[Setup]
AppId={{6F3C2A51-7B1D-4C8E-9A42-5D1E0B7C3F10}
AppName=Musically Unorthodox
AppVersion={#AppVersion}
AppPublisher=Musically Unorthodox
AppPublisherURL=https://musicallyunorthodox.com
DefaultDirName={autopf}\Musically Unorthodox
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=MusicallyUnorthodox-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Musically Unorthodox
CloseApplications=no

[Messages]
WelcomeLabel2=This installs the Musically Unorthodox plugin (VST3) and the standalone app.%n%nClose your music software first. When it has finished, open your music software again and rescan plugins if it does not show up.

[Files]
Source: "{#Artefacts}\VST3\Musically Unorthodox.vst3\*"; DestDir: "{commoncf64}\VST3\Musically Unorthodox.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Musically Unorthodox.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Musically Unorthodox"; Filename: "{app}\Musically Unorthodox.exe"
