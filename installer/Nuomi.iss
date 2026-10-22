; ============================================================================
;  Nuomi 安装包脚本（Inno Setup）
;  用法：用 Inno Setup Compiler 打开本文件 -> Build。产出 installer/out/Nuomi-Setup.exe
;
;  行为：
;   1. 装到 %LOCALAPPDATA%\Programs\Nuomi（用户级，无需管理员）
;   2. 自动把该目录加入「用户 PATH」（HKCU\Environment），新开 cmd 直接打 nuomi
;   3. 开始菜单 + 可选桌面快捷方式
;   4. 卸载时自动从 PATH 移除并删目录
; ============================================================================

#define MyAppName      "Nuomi"
#define MyAppVersion   "0.1.0"
#define MyAppPublisher "Nuomi Lang"
#define MyAppURL       "https://github.com/Ryan-cpu-lang/Nuomi"

[Setup]
; AppId 用双花括号转义，避免被当成 Inno 常量
AppId={{8F3C5A1E-2B7D-4C9A-9E6F-1A2B3C4D5E6F}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
; 用户级安装，不请求管理员
DefaultDirName={localappdata}\Programs\Nuomi
DefaultGroupName={#MyAppName}
PrivilegesRequired=lowest
ChangesEnvironment=yes
; 不设 SourceDir：所有相对路径都以本 .iss 所在目录为基准，避免 OutputDir 跟着 SourceDir 偏移
OutputDir=out
OutputBaseFilename=Nuomi-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 安装器 / 卸载器 / 快捷方式统一用 Nuomi 麻糬图标
SetupIconFile=..\nuomi.ico
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\nuomi.ico

[Languages]
Name: "cn"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\windows\nuomi.exe";   DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\windows\nucutil.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\nuomi.ico";                DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md";  DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\LICENSE";    DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Nuomi 命令行"; Filename: "{app}\nuomi.exe"; \
      IconFilename: "{app}\nuomi.ico"; Comment: "运行 Nuomi 脚本（nuomi xxx.nuomi）"
Name: "{group}\Nuomi 官网"; Filename: "{#MyAppURL}"
Name: "{autodesktop}\Nuomi"; Filename: "{app}\nuomi.exe"; \
      IconFilename: "{app}\nuomi.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "额外选项："

[Code]
{ PATH 变更后的 WM_SETTINGCHANGE 广播由 [Setup] 的 ChangesEnvironment=yes 自动完成，
  这里只负责读写 HKCU\Environment 的 Path }

procedure AddToPath();
var
  Path, NewPath, Dir: string;
begin
  Dir := ExpandConstant('{app}');
  if RegQueryStringValue(HKCU, 'Environment', 'Path', Path) then
  begin
    { 已经加过就跳过 }
    if (Pos(';' + Dir + ';', ';' + Path + ';') > 0) then Exit;
    if Path = '' then
      NewPath := Dir
    else if (Copy(Path, Length(Path), 1) = ';') then
      NewPath := Path + Dir
    else
      NewPath := Path + ';' + Dir;
    RegWriteStringValue(HKCU, 'Environment', 'Path', NewPath);
  end
  else
    RegWriteStringValue(HKCU, 'Environment', 'Path', Dir);
end;

procedure RemoveFromPath();
var
  Path, NewPath, Dir: string;
  P, L: Integer;
begin
  Dir := ExpandConstant('{app}');
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', Path) then Exit;
  P := Pos(';' + Dir + ';', ';' + Path + ';');
  if P = 0 then Exit;
  L := Length(Dir);
  { 把 ";Dir;" 整段去掉，再修掉可能产生的首尾多余分号 }
  NewPath := Copy(Path, 1, P - 1) + Copy(Path, P + L + 2, MaxInt);
  if (NewPath <> '') and (Copy(NewPath, 1, 1) = ';') then
    NewPath := Copy(NewPath, 2, MaxInt);
  if (NewPath <> '') and (Copy(NewPath, Length(NewPath), 1) = ';') then
    NewPath := Copy(NewPath, 1, Length(NewPath) - 1);
  if NewPath = '' then
    RegDeleteValue(HKCU, 'Environment', 'Path')
  else
    RegWriteStringValue(HKCU, 'Environment', 'Path', NewPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then AddToPath();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then RemoveFromPath();
end;
