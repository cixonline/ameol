#include "..\Shared\include\version.bld"
#define ExeName "Ameol232.exe"

[Setup]
InternalCompressLevel=ultra
OutputDir=..\drops
MinVersion=0,5.1.2600
OutputBaseFilename=a{#PRODUCT_MAX_VER}.{#PRODUCT_MIN_VER}.{#PRODUCT_BUILD}
AppCopyright=CIX
AppName=Ameol
AppVerName={#PRODUCT_MAX_VER}.{#PRODUCT_MIN_VER}.{#PRODUCT_BUILD}
DefaultDirName={reg:HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Ameol232.exe,Path|{sd}\Ameol2}
SourceDir=.\
UserInfoPage=true
DefaultUserInfoName={reg:HKLM\SOFTWARE\CIX\Ameol2\User Info,Username|{sysuserinfoname}}
DefaultUserInfoOrg={reg:HKLM\SOFTWARE\CIX\Ameol2\User Info,Sitename|{sysuserinfoorg}}
VersionInfoVersion={#PRODUCT_MAX_VER}.{#PRODUCT_MIN_VER}.{#PRODUCT_BUILD}
VersionInfoCompany=CIX
VersionInfoDescription=Ameol Setup Program
WizardImageFile=common\instwiza.bmp
WizardSmallImageFile=common\instwiz2-small.bmp
SetupIconFile=common\a2setup2.ico
AppPublisher=CIX Ltd
AppPublisherURL=www.cix.uk
AppSupportURL=www.cix.uk
AppUpdatesURL=www.cix.uk
AppVersion={#PRODUCT_BUILD}
UninstallDisplayName=Ameol Off Line Reader for CIX
DirExistsWarning=no
AppMutex=UniqueAmeol2Name
DefaultGroupName=CIX Software
AllowNoIcons=true
InfoBeforeFile=
ShowLanguageDialog=no
InfoAfterFile=
MergeDuplicateFiles=true
UninstallDisplayIcon={app}\{#ExeName}
UninstallLogMode=overwrite

[Messages]
BeveledLabel=CIX Ltd

[Types]
Name: full; Description: Full installation
Name: addons; Description: Install Only CIX Addons
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: main; Description: Ameol Program; Types: full custom
Name: addons; Description: CIX Addons (Moderate + Decoder); Types: full addons custom
Name: tutorials; Description: Ameol Tutorials; Types: full custom

[Tasks]
Name: desktopicon; Description: Add a shortcut to the Desktop; Components: main
Name: quickstart; Description: Add a shortcut to the Quick Launch toolbar; Components: main

[Icons]
Name: {group}\Ameol2; Filename: {app}\{#ExeName}; WorkingDir: {app}; Components: main
Name: {group}\Ameol2 Setup; Filename: {app}\{#ExeName}; Parameters: /setup; WorkingDir: {app}; Components: main; IconFilename: {app}\A2Setup2.ico
Name: {userdesktop}\CIX Software; Filename: {app}\CIX Software; WorkingDir: {app}\CIX Software; IconFilename: {app}\am2fold.ico; Components: main; Tasks: desktopicon
Name: {app}\CIX Software\Ameol2; Filename: {app}\{#ExeName}; WorkingDir: {app}; Components: main
Name: {app}\CIX Software\Ameol2 Setup; Filename: {app}\{#ExeName}; Parameters: /setup; WorkingDir: {app}; Components: main; IconFilename: {app}\A2Setup2.ico
Name: {userappdata}\Microsoft\Internet Explorer\Quick Launch\Ameol2; Filename: {app}\{#ExeName}; WorkingDir: {app}; Components: main; Tasks: quickstart

[InstallDelete]
Type: files; Name: "{userdesktop}\CIX Software.lnk"
Type: files; Name: "{app}\CIX Software\Ameol2 32-bit.lnk"
Type: files; Name: "{app}\CIX Software\Ameol2 32-bit Setup.lnk"
Type: files; Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Ameol2.lnk"
Type: files; Name: "{group}\Ameol2 32-bit.lnk"
Type: files; Name: "{group}\Ameol2 32-bit Setup.lnk"
Type: files; Name: "{app}\WhatsNew.cnt"
Type: files; Name: "{app}\WhatsNew.fts"
Type: files; Name: "{app}\WhatsNew.hlp"
Type: files; Name: "{app}\WhatsNew254.fts"
Type: files; Name: "{app}\WhatsNew254.hlp"
Type: files; Name: "{app}\WhatsNew255.fts"
Type: files; Name: "{app}\WhatsNew255.hlp"
Type: files; Name: "{app}\SignUp.hlp"
Type: files; Name: "{app}\changes.txt"
Type: files; Name: "{app}\CIXHelp.hlp"
Type: files; Name: "{app}\chfix254.txt"
Type: files; Name: "{app}\Ameol2.hlp"
Type: files; Name: "{app}\Licence.txt"
Type: files; Name: "{app}\Addons\Moderate.hlp"
Type: files; Name: "{app}\Addons\cixbill.hlp"
Type: files; Name: "{app}\Addons\*.gid"
Type: files; Name: "{app}\*.gid"

[Dirs]
Name: {app}\Dict
Name: {app}\Addons
Name: {app}\Billing
Name: {app}\Forms
Name: {app}\Mugshot
Name: {app}\Resumes
Name: {app}\Scratch
Name: {app}\Script; Flags: uninsalwaysuninstall
Name: {app}\Sounds; Flags: uninsalwaysuninstall
Name: {app}\Users
Name: {app}\CIX Software; Flags: uninsalwaysuninstall

[Run]
Filename: {app}\{#ExeName}; Flags: nowait postinstall unchecked; Components: main; WorkingDir: {app}; Description: Run Ameol now; StatusMsg: Launching Ameol; Parameters: {reg:HKLM\SOFTWARE\CIX\Ameol2\User Info,DefaultUser|} {reg:HKLM\SOFTWARE\CIX\Ameol2\User Info,Command Line|}

[INI]
Filename: {app}\Scripts.ini; Section: Scripts; Key: aboutme.scr; String: "Get CIX ""allaboutme"" information"; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: absence.scr; String: Set CIX absence notification; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: banlist.scr; String: Download forum ban list; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: billing.scr; String: Get CIX billing details; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: def_prof.scr; String: Install default profile; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: deb_prof.scr; String: Install debugging profile; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: fal.scr; String: List files in an archive on CIX; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: ffind.scr; String: CIX FFIND command; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: filestat.scr; String: CIX FILESTATS command; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: findpgpk.scr; String: Find a PGP keyfile; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: finduser.scr; String: Find CIX User information; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: getall.scr; String: Consolidate all forums; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: getconf.scr; String: Show Forum Moderators; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: getrange.scr; String: Get a range of messages from CIX; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: gettime.scr; String: Get PC time/date from CIX; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: mailfile.scr; String: Mail a local file; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: myconfs.scr; String: Download a list of joined forums; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: ngshow.scr; String: Show joined groups in Newsnet; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: password.scr; String: Change your CIX password; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: rssadd.scr; String: Create an RSS feed for CIX; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: profget.scr; String: Get profile.txt; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: profput.scr; String: Put profile.txt; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: reset.scr; String: Reset CIX Forums pointers back a number of days; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: restore.scr; String: Restore pointers; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: search.scr; String: Text search on CIX; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: skipback.scr; String: Skip back a few messages in a forum/topic; Flags: createkeyifdoesntexist
Filename: {app}\Scripts.ini; Section: Scripts; Key: userban.scr; String: Retrieve the list of forums you are banned from; Flags: createkeyifdoesntexist
Filename: {app}\nocommnt.ini; Section: noticeboard; Key: NoComment; String: TRUE; Flags: createkeyifdoesntexist
Filename: {app}\nocommnt.ini; Section: pc.noticeboard; Key: NoComment; String: TRUE; Flags: createkeyifdoesntexist

[Files]
; Ameol Core Files
Source: source\{#ExeName}; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\Amdb32.dll; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\Amctrl32.dll; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\Ameol32.exe; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\Amob32.dll; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\Amuser32.dll; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\Zmodem32.afp; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\ameol232.exe.manifest; DestDir: {app}; Components: main; Flags: onlyifdoesntexist
Source: source\RegExp.dll; DestDir: {app}; Components: main; Flags: ignoreversion; MinVersion: 4.1.1998,4.0.1381sp6
Source: source\RegExp9x.dll; DestDir: {app}; Components: main; Flags: ignoreversion; DestName: RegExp.dll; OnlyBelowVersion: 4.1.1998,4.0.1381sp6
Source: source\SciLexer.dll; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\winsparkle.dll; DestDir: {app}; Components: main; Flags: ignoreversion

; New ZModem II module
Source: source\nwzmod32.afp; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\addons\ZMCFG32.ADN; DestDir: {app}\Addons; Components: main; Flags: ignoreversion

; Help Files
Source: source\ameol2.chm; DestDir: {app}; Components: main

; Readme, Licence etc
Source: source\changes.html; DestDir: {app}; Components: main
Source: source\tutorial.scr; DestDir: {app}; Components: tutorials

; Icons
Source: source\am2fold.ico; DestDir: {app}; Components: main
Source: source\A2Setup2.ico; DestDir: {app}; Components: main

; Spell Checker etc
Source: source\ssce32.dll; DestDir: {app}; Components: main; Flags: ignoreversion
Source: source\DICT\Ssceamm.clx; DestDir: {app}\Dict; Components: main
Source: source\DICT\SSCEBRM.CLX; DestDir: {app}\Dict; Components: main
Source: source\DICT\Ssceamc.tlx; DestDir: {app}\Dict; Components: main
Source: source\DICT\SSCEBRC.TLX; DestDir: {app}\Dict; Components: main

; Additional Dictionaries
Source: source\ACRONYMS.LST; DestDir: {app}; Components: main; Flags: comparetimestamp
Source: source\DICT\ACRONYMS.DIC; DestDir: {app}\Dict; Components: main; Flags: comparetimestamp
Source: source\DICT\Comptrm1.dic; DestDir: {app}\Dict; Components: main
Source: source\DICT\meditrm.dic; DestDir: {app}\Dict; Components: main
Source: source\DICT\places.dic; DestDir: {app}\Dict; Components: main

; INI & Script Files
#if IS_BETA==1
Source: source\beta_report.ini; DestDir: {app}; DestName: report.ini; Components: main; Flags: overwritereadonly replacesameversion
#else
Source: source\report.ini; DestDir: {app}; DestName: report.ini; Components: main; Flags: overwritereadonly replacesameversion
#endif
Source: source\Script\*.*; DestDir: {app}\Script; Components: main
Source: source\cix.dat; DestDir: {app}; Components: main
Source: source\sounds\*.*; DestDir: {app}\Sounds; Components: main
Source: source\addrbook.def; DestDir: {app}; Components: main
Source: source\cats.def; DestDir: {app}; Components: main
Source: source\tips.def; DestDir: {app}; Components: main

; Windows Specific Files
Source: source\msvcrt.dll; DestDir: {app}; Components: main

; Moderate Addon
Source: source\addons\librarys.txt; DestDir: {app}\Addons; Components: addons
Source: source\addons\changes.txt; DestDir: {app}\Addons; Components: addons
Source: source\addons\moderate.chm; DestDir: {app}\Addons; Components: addons
Source: source\addons\modera32.adn; DestDir: {app}\Addons; Components: addons; Flags: ignoreversion

; Decoder addon
Source: source\addons\Decoder.adn; DestDir: {app}\Addons; Components: addons; Flags: ignoreversion; MinVersion: 4.1.1998,4.0.1381sp6; Tasks: 

; GetMM addon
Source: source\addons\GetMM32.adn; DestDir: {app}\Addons; Components: addons 
Source: source\addons\GetMM.hlp; DestDir: {app}\Addons; Components: addons 
Source: source\addons\GetMM32.txt; DestDir: {app}\Addons; Components: addons 

[UninstallDelete]
Name: {app}\nocommnt.ini; Type: files
Name: {app}\Scripts.ini; Type: files
Name: {app}\Addons; Type: filesandordirs
Name: {app}\DICT; Type: filesandordirs
Name: {app}\*.log; Type: files
Name: {app}\*.txt; Type: files
Name: {app}\*.scr; Type: files
Name: {app}\*.manifest; Type: files
Name: {app}\*.lst; Type: files
Name: {app}\*.dll; Type: files
Name: {app}\Data; Type: dirifempty
Name: {app}\Scratch; Type: dirifempty
Name: {app}\Resumes; Type: dirifempty
Name: {app}\Mugshot; Type: dirifempty
Name: {app}\Billing; Type: dirifempty
Name: {app}\Forms; Type: dirifempty
Name: {app}; Type: dirifempty


[Registry]
Root: HKLM; Subkey: Software\CIX\Ameol2; ValueType: string; Flags: uninsdeletekey; Components: main
Root: HKCU; Subkey: Software\CIX\Ameol2; ValueType: string; Flags: uninsdeletekey; Components: main

Root: HKLM; Subkey: SOFTWARE\CIX\Ameol2\User Info; Permissions: users-full; Components: main
Root: HKLM; Subkey: SOFTWARE\CIX\Ameol2\User Info; ValueType: string; ValueName: Username; ValueData: {userinfoname}; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: SOFTWARE\CIX\Ameol2\User Info; ValueType: string; ValueName: Sitename; ValueData: {userinfoorg}; Flags: uninsdeletekey; Components: main

Root: HKLM; Subkey: Software\CIX\Ameol2\Export; ValueType: string; ValueName: Ameol Scratchpad; ValueData: {#ExeName}|CIXExport; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Export; ValueType: string; ValueName: Text File; ValueData: {#ExeName}|TextFileExport; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Export; ValueType: string; ValueName: Address Book; ValueData: {#ExeName}|Amaddr_CSVExport; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Export; ValueType: none; ValueName: CIX Scratchpad; ValueData: none; Flags: deletevalue; Components: main

Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: CIX Scratchpad; ValueData: {#ExeName}|CIXImport; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: Address Book; ValueData: {#ExeName}|Amaddr_Import; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: CIX Forums List; ValueData: {#ExeName}|CIXList_Import; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: CIX Users List; ValueData: {#ExeName}|UserList_Import; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: CIX Newsgroup List; ValueData: {#ExeName}|UsenetList_Import; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: Ameol Scratchpad; ValueData: {#ExeName}|AmeolImport; Flags: uninsdeletekey; Components: main
Root: HKLM; Subkey: Software\CIX\Ameol2\Import; ValueType: string; ValueName: Ameol Import Wizard; ValueData: {#ExeName}|AmdbImportWizard; Flags: uninsdeletekey; Components: main

Root: HKLM; Subkey: Software\Clients\Mail\Ameol2; ValueType: string; ValueData: Ameol2; Components: main; Flags: uninsdeletekey
Root: HKLM; Subkey: Software\Clients\Mail\Ameol2\Protocols\mailto; ValueType: string; ValueData: URL:MailTo Protocol; Components: main
Root: HKLM; Subkey: Software\Clients\Mail\Ameol2\Protocols\mailto; ValueType: string; ValueName: URL Protocol; Components: main
Root: HKLM; Subkey: Software\Clients\Mail\Ameol2\Protocols\mailto; ValueType: binary; ValueName: EditFlags; ValueData: 02 00 00 00; Components: main
Root: HKLM; Subkey: Software\Clients\Mail\Ameol2\Protocols\mailto\DefaultIcon; ValueType: string; ValueData: {app}\{#ExeName},6; Components: main
Root: HKLM; Subkey: Software\Clients\Mail\Ameol2\Protocols\mailto\Shell\Open\Command; ValueType: string; ValueData: """{app}\{#ExeName}"" /mailto=""%1"""; Components: main
Root: HKLM; Subkey: Software\Clients\Mail\Ameol2\Shell\Open\Command; ValueType: string; ValueData: {app}\{#ExeName}; Components: main

Root: HKLM; Subkey: Software\Clients\News\Ameol2; ValueType: string; ValueData: Ameol2; Components: main; Flags: uninsdeletekey
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\nntp; ValueType: string; ValueData: URL:NNTP Protocol; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\nntp; ValueType: string; ValueName: URL Protocol; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\nntp; ValueType: binary; ValueName: EditFlags; ValueData: 02 00 00 00; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\nntp\DefaultIcon; ValueType: string; ValueData: {app}\{#ExeName},4; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\nntp\Shell\Open\Command; ValueType: string; ValueData: """{app}\{#ExeName}"" /news=""%1"""; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Shell\Open\Command; ValueType: string; ValueData: {app}\{#ExeName}; Components: main

Root: HKLM; Subkey: Software\Clients\News\Ameol2; ValueType: string; ValueData: Ameol2; Components: main; Flags: uninsdeletekey
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\news; ValueType: string; ValueData: URL:News Protocol; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\news; ValueType: string; ValueName: URL Protocol; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\news; ValueType: binary; ValueName: EditFlags; ValueData: 02 00 00 00; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\news\DefaultIcon; ValueType: string; ValueData: {app}\{#ExeName},4; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Protocols\news\Shell\Open\Command; ValueType: string; ValueData: """{app}\{#ExeName}"" /news=""%1"""; Components: main
Root: HKLM; Subkey: Software\Clients\News\Ameol2\Shell\Open\Command; ValueType: string; ValueData: {app}\{#ExeName}; Components: main

Root: HKCR; Subkey: cix; ValueType: string; ValueData: URL:CIX Forums; Components: main; Flags: uninsdeletekey
Root: HKCR; Subkey: cix\URL Protocol; ValueType: string; Components: main
Root: HKCR; Subkey: cix\DefaultIcon; ValueType: string; ValueData: {app}\{#ExeName},5; Components: main
Root: HKCR; Subkey: cix\Shell\Open\Command; ValueType: string; ValueData: """{app}\{#ExeName}"" /cix=""%1"""; Components: main

Root: HKCR; Subkey: cixfile; ValueType: string; ValueData: URL:CIX Forums File; Components: main; Flags: uninsdeletekey
Root: HKCR; Subkey: cixfile\URL Protocol; ValueType: string; Components: main
Root: HKCR; Subkey: cixfile\DefaultIcon; ValueType: string; ValueData: {app}\{#ExeName},25; Components: main
Root: HKCR; Subkey: cixfile\Shell\Open\Command; ValueType: string; ValueData: """{app}\{#ExeName}"" /cixfile=""%1"""; Components: main

Root: HKCU; Subkey: AppEvents\EventLabels\AmCheckComplete; ValueType: string; ValueData: Check Complete; Components: main; Flags: uninsdeletekey
Root: HKCU; Subkey: AppEvents\EventLabels\AmClose; ValueType: string; ValueData: Close; Components: main; Flags: uninsdeletekey
Root: HKCU; Subkey: AppEvents\EventLabels\AmNewMail; ValueType: string; ValueData: New Mail; Components: main; Flags: uninsdeletekey
Root: HKCU; Subkey: AppEvents\EventLabels\AmOpen; ValueType: string; ValueData: Open; Components: main
Root: HKCU; Subkey: AppEvents\EventLabels\AmParseComplete; ValueType: string; ValueData: Parse Complete; Components: main; Flags: uninsdeletekey

Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Check Complete\.Current; ValueType: string; ValueData: {app}\Sounds\check.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Check Complete\.Default; ValueType: string; ValueData: {app}\Sounds\check.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Close\.Current; ValueType: string; Components: main
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Close\.Default; ValueType: string; Components: main
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\New Mail\.Current; ValueType: string; ValueData: {app}\Sounds\newmail.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\New Mail\.Default; ValueType: string; ValueData: {app}\Sounds\newmail.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Open\.Current; ValueType: string; Components: main
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Open\.Default; ValueType: string; Components: main
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Parse Complete\.Current; ValueType: string; ValueData: {app}\Sounds\newmes.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Parse Complete\.Default; ValueType: string; ValueData: {app}\Sounds\newmes.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Purge Complete\.Current; ValueType: string; ValueData: {app}\Sounds\purge.wav; Components: main; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2\Purge Complete\.Default; ValueType: string; ValueData: {app}\Sounds\purge.wav; Components: main; Flags: createvalueifdoesntexist

Root: HKLM; Subkey: Software\Microsoft\Plus!\System Agent\SAGE\Ameol2; ValueType: string; ValueName: Friendly Name; ValueData: Ameol Off-Line Reader; OnlyBelowVersion: 0,0; Components: main; Flags: uninsdeletekey
Root: HKLM; Subkey: Software\Microsoft\Plus!\System Agent\SAGE\Ameol2; ValueType: string; ValueName: Program; ValueData: {app}\{#ExeName}; OnlyBelowVersion: 0,0; Components: main
Root: HKLM; Subkey: Software\Microsoft\Plus!\System Agent\SAGE\Ameol2; ValueType: dword; ValueName: Settings; ValueData: 1; OnlyBelowVersion: 0,0; Components: main

Root: HKLM; Subkey: SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Ameol2; Flags: deletekey deletevalue; Components: main
Root: HKLM; Subkey: SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Ameol232.exe; Flags: uninsdeletekey; Components: main
Root: HKCU; Subkey: AppEvents\Schemes\Apps\Ameol2; Flags: uninsdeletekey; Components: main
Root: HKCU; Subkey: Software\CIX; Flags: uninsdeletekey
Root: HKLM; Subkey: SOFTWARE\CIX; Flags: uninsdeletekey

[Code]
var
  rNames: TArrayOfString;
  FChecked: Boolean;
  FLastPage: Integer;

procedure CurPageChanged(CurPage: Integer);
begin
  FLastPage := CurPage;
end;

procedure DeInitializeSetup();
VAR
  i: Integer;
  lPath:String;
  lApp:String;
begin

	if FLastPage=wpFinished then begin
		lApp := ExpandConstant('{app}')+'\Addons\';
	//	Strobe the registry for existing users
		RegGetSubkeyNames(HKEY_CURRENT_USER,'Software\CIX\Ameol2',rNames);
		for i := 0 to GetArrayLength(rNames) - 1  do begin
			lPath := 'Software\CIX\Ameol2\' + rNames[i] + '\Directories';
			if RegValueExists(HKEY_CURRENT_USER, lPath, 'Addons') then begin
				lPath := 'Software\CIX\Ameol2\' + rNames[i] + '\Addons';
				if POS('addons', WizardSelectedComponents(False)) > 0 then begin
					if FileExists(lApp + 'MODERA32.ADN') then
						RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'MODERA32', lApp + 'MODERA32.ADN');
					if FileExists(lApp + 'DECODER.ADN') then
						RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'DECODER',  lApp + 'DECODER.ADN');
					if FileExists(lApp + 'GETMM32.ADN') then
						RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'GETMM32',  lApp + 'GETMM32.ADN');
				end;
				if POS('main', WizardSelectedComponents(False)) > 0 then begin
					RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'ZMCFG32',  lApp + 'ZMCFG32.ADN');
				end;
			end;
		end;
//	Always Add Admin in case it's a new setup
		lPath := 'Software\CIX\Ameol2\ADMIN\Addons';
		if POS('addons', WizardSelectedComponents(False)) > 0 then begin
			if FileExists(lApp + 'MODERA32.ADN') then
				RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'MODERA32', lApp + 'MODERA32.ADN');
			if FileExists(lApp + 'DECODER.ADN') then
				RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'DECODER',  lApp + 'DECODER.ADN');
			if FileExists(lApp + 'GETMM32.ADN') then
				RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'GETMM32',  lApp + 'GETMM32.ADN');
		end;
		if POS('main', WizardSelectedComponents(False)) > 0 then begin
			RegWriteStringValue (HKEY_CURRENT_USER, lPath, 'ZMCFG32',  lApp + 'ZMCFG32.ADN');
		end;
		RegWriteStringValue (HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Ameol232.exe', '', ExpandConstant('{app}') + '\Ameol232.exe');
		RegWriteStringValue (HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Ameol232.exe', 'Path', ExpandConstant('{app}'));

	End;
end;

function Ameol2Exists(pFileName: String): Boolean;
begin
	if not FChecked then begin
		FChecked := True;
		if FileExists(pFileName) then begin
			Result := MsgBox('Do you want to re-install the Ameol tutorials?', mbConfirmation, MB_YESNO) = idYes;
		end;
	end;
end;

Function GetAppPath(pDefault:String):String;
VAR
  lPath:  String;
begin
    if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Ameol232.exe', 'Path', lPath) then begin
		if Pos('AMEOL232.EXE', Uppercase(lPath)) > 0 then
		  Result := ExtractFilePath(lPath)
		else
		  Result := lPath;
    end else begin
		Result := pDefault;
	end;
	Result := AddBackslash(Result);
end;

end.
