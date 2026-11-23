[Setup]
AppId={{C7F2B41D-8E33-4A9C-B142-7D6F9A23E510}
AppName=Shanty Sonar EQ
AppVersion=1.0.0
AppPublisher=Adamsky Music
DefaultDirName={commonpf64}\Common Files\VST3
DisableDirPage=yes
DefaultGroupName=Shanty Sonar EQ
OutputDir=..\dist
OutputBaseFilename=ShantySonarEQ-1.0.0-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full installation (VST3 + Standalone)"
Name: "vst3only"; Description: "VST3 Plugin only"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Shanty Sonar EQ VST3 Plugin (64-bit)"; Types: full vst3only custom; Flags: fixed
Name: "standalone"; Description: "Shanty Sonar EQ Standalone App"; Types: full custom

[Files]
Source: "..\build-win\ShantySonarEQ_artefacts\Release\VST3\Shanty Sonar EQ.vst3\*"; DestDir: "{commoncf64}\VST3\Shanty Sonar EQ.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build-win\ShantySonarEQ_artefacts\Release\Standalone\Shanty Sonar EQ.exe"; DestDir: "{commonpf64}\Adamsky Music\Shanty Sonar EQ"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\Shanty Sonar EQ"; Filename: "{commonpf64}\Adamsky Music\Shanty Sonar EQ\Shanty Sonar EQ.exe"; Components: standalone
Name: "{group}\Uninstall Shanty Sonar EQ"; Filename: "{uninstallexe}"
