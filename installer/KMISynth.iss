; KMISynth installer (Inno Setup 6). Built by scripts\make-installer.ps1 or the CI workflow:
;   ISCC.exe /DAppVersion=0.1.0 /DSourceDir=<...\KMISynth_artefacts\Release> installer\KMISynth.iss
; Installs the VST3 bundle to Common Files\VST3 and, optionally, the standalone app. User presets in
; Documents\KMISynth are never touched (also not on uninstall).

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\plugin\KMISynth_artefacts\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif

[Setup]
AppId={{8C4F2E6B-5D1A-4F7B-9E3C-2A6D8B1F0C47}
AppName=KMISynth
AppVersion={#AppVersion}
AppVerName=KMISynth {#AppVersion}
AppPublisher=KMI
AppPublisherURL=https://github.com/KellyMurphy/KMISynth
AppSupportURL=https://github.com/KellyMurphy/KMISynth/issues
DefaultDirName={autopf}\KMISynth
DefaultGroupName=KMISynth
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=KMISynth-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=KMISynth {#AppVersion}
UninstallDisplayIcon={app}\KMISynth.exe

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "vst3"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Common Files\VST3\KMISynth.vst3)"; Types: full vst3 custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#SourceDir}\VST3\KMISynth.vst3\*"; DestDir: "{commoncf64}\VST3\KMISynth.vst3"; Components: vst3; \
    Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\Standalone\KMISynth.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\KMISynth"; Filename: "{app}\KMISynth.exe"; Components: standalone
Name: "{group}\Uninstall KMISynth"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\KMISynth.exe"; Description: "Start KMISynth now"; Components: standalone; Flags: nowait postinstall skipifsilent
