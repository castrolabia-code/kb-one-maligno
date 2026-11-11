; Instalador de KB ONE Maligno (Inno Setup 6.3+).
; Lo compila GitHub Actions (ver .github/workflows/windows-build.yml).
; Instala el .vst3 en C:\Program Files\Common Files\VST3\ y el Standalone
; en C:\Program Files\Castro Lab\KB ONE Maligno\.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{7061C437-EBF9-4DFE-934D-103206BBD9FA}
AppName=KB ONE Maligno
AppVersion={#AppVersion}
AppPublisher=Castro Lab
DefaultDirName={autopf}\Castro Lab\KB ONE Maligno
DisableProgramGroupPage=yes
DisableDirPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\..\build\installer
OutputBaseFilename=KB-ONE-Maligno-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=KB ONE Maligno

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
; Borra una versión anterior del plugin antes de copiar la nueva.
Type: filesandordirs; Name: "{commoncf64}\VST3\KB ONE Maligno.vst3"

[Files]
Source: "..\..\build\KBOne_artefacts\Release\VST3\KB ONE Maligno.vst3\*"; DestDir: "{commoncf64}\VST3\KB ONE Maligno.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\build\KBOne_artefacts\Release\Standalone\KB ONE Maligno.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\KB ONE Maligno"; Filename: "{app}\KB ONE Maligno.exe"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\KB ONE Maligno.vst3"
