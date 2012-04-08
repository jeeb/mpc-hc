; Translation made with Stonevoice Translator 2.1 (http://www.stonevoice.com/auto/translator)
; $Translator:NL=%n:TB=%t
; *** Inno Setup version 5.1.11+ Turkish messages ***
; Language     " T�k�e"               ::::::    Turkish
; Translate by " �eviren "            ::::::    Adil YILDIZ (�evirmen) & Sinan Hunerel (�zelle�tirme)
; E-Mail       " Elektronik Posta "   ::::::    adil@kde.org.tr
; Home Page    " Web Adresi "         ::::::    http://www.adilyildiz.com.tr
;
; $jrsoftware: issrc/Files/Default.isl,v 1.66 2005/02/25 20:23:48 mlaan Exp $
[LangOptions]
LanguageName=T<00FC>rk<00E7>e
LanguageID=$041f
LanguageCodePage=1254
; If the language you are translating to requires special font faces or
; sizes, uncomment any of the following entries and change them accordingly.
;DialogFontName=
;DialogFontSize=8
;WelcomeFontName=Verdana
;WelcomeFontSize=12
;TitleFontName=Arial
;TitleFontSize=29
;CopyrightFontName=Arial
;CopyrightFontSize=8
DialogFontName=
[Messages]

; *** Application titles
SetupAppTitle=Kurulum
SetupWindowTitle=%1 - Kurulumu
UninstallAppTitle=Kald�rma
UninstallAppFullTitle=%1 Uygulamas�n� Kald�r

; *** Misc. common
InformationTitle=Bilgi
ConfirmTitle=Sorgu
ErrorTitle=Hata

; *** SetupLdr messages
SetupLdrStartupMessage=Bu kurulum %1 program�n� y�kleyecektir. Devam etmek istiyor musunuz?
LdrCannotCreateTemp=Ge�ici bir dosya olu�turulamad�. Kurulum iptal edildi
LdrCannotExecTemp=Ge�ici dizindeki dosya �al��t�r�lamad�. Kurulum iptal edildi

; *** Startup error messages
LastErrorMessage=%1.%n%nHata %2: %3
SetupFileMissing=%1 adl� dosya kurulum dizininde bulunamad�. L�tfen problemi d�zeltiniz veya program�n yeni bir kopyas�n� edininiz.
SetupFileCorrupt=Kurulum dosyalar� bozulmu�. L�tfen program�n yeni bir kopyas�n� edininiz.
SetupFileCorruptOrWrongVer=Kurulum dosyalar� bozulmu� veya kurulumun bu s�r�m� ile uyu�muyor olabilir. L�tfen problemi d�zeltiniz veya Program�n yeni bir kopyas�n� edininiz.
NotOnThisPlatform=Bu program %1 �zerinde �al��t�r�lamaz.
OnlyOnThisPlatform=Bu program sadece %1 �zerinde �al��t�r�lmal�d�r.
OnlyOnTheseArchitectures=Bu program sadece a�a��daki mimarilere sahip Windows s�r�mlerinde �al���r:%n%n%1
MissingWOW64APIs=Kulland���n�z Windows s�r�m� Kur'un 64-bit y�kleme yapabilmesi i�in gerekli olan �zelliklere sahip de�ildir. Bu problemi ortadan kald�rmak i�in l�tfen Service Pack %1 y�kleyiniz.
WinVersionTooLowError=Bu program� �al��t�rabilmek i�in %1 %2 s�r�m� veya daha sonras� y�kl� olmal�d�r.
WinVersionTooHighError=Bu program %1 %2 s�r�m� veya sonras�nda �al��maz.
AdminPrivilegesRequired=Bu program kurulurken y�netici olarak oturum a��lm�� olmak gerekmektedir.
PowerUserPrivilegesRequired=Bu program kurulurken Y�netici veya G�� Y�neticisi Grubu �yesi olarak giri� yap�lm�� olmas� gerekmektedir.
SetupAppRunningError=Kur %1 program�n�n �al��t���n� tespit etti.%n%nL�tfen bu program�n �al��an b�t�n par�alar�n� �imdi kapat�n�z, daha sonra devam etmek i�in Tamam'a veya ��kmak i�in �ptal'e bas�n�z.
UninstallAppRunningError=Kald�r %1 program�n�n �al��t���n� tespit etti.%n%nL�tfen bu program�n �al��an b�t�n par�alar�n� �imdi kapat�n�z, daha sonra devam etmek i�in Tamam'a veya ��kmak i�in �ptal'e bas�n�z.

; *** Misc. errors
ErrorCreatingDir=Kur " %1 " dizinini olu�turamad�.
ErrorTooManyFilesInDir=" %1 " dizininde bir dosya olu�turulamad�. ��nk� dizin �ok fazla dosya i�eriyor

; *** Setup common messages
ExitSetupTitle=Kurulumdan ��k��
ExitSetupMessage=Kurulum hen�z tamamlanmad�.%n%nKurulumu tekrar �al��t�rarak y�kleme i�lemini tamamlayabilirsiniz.%n%nKurulumdan ��kmak istedi�inizden emin misiniz?
AboutSetupMenuItem=Kur H&akk�nda...
AboutSetupTitle=Kur Hakk�nda
AboutSetupMessage=%1 %2 s�r�m�%n%3%n%n%1 internet:%n%4
AboutSetupNote=
TranslatorNote=�yi bir kurulum program� ar�yorsan�z buldunuz...%nhttp://www.adilyildiz.com.tr

; *** Buttons
ButtonBack=< G&eri
ButtonNext=�&leri >
ButtonInstall=&Kur
ButtonOK=Tamam
ButtonCancel=�ptal
ButtonYes=E&vet
ButtonYesToAll=T�m�ne E&vet
ButtonNo=&Hay�r
ButtonNoToAll=T�m�ne Ha&y�r
ButtonFinish=&Son
ButtonBrowse=&G�zat...
ButtonWizardBrowse=G�za&t...
ButtonNewFolder=Ye&ni Dizin Olu�tur

; *** "Select Language" dialog messages
SelectLanguageTitle=Kur Dilini Se�iniz
SelectLanguageLabel=L�tfen kurulum s�ras�nda kullanaca��n�z dili se�iniz:

; *** Common wizard text
ClickNext=Devam etmek i�in �leri, ��kmak i�in �ptal tu�una bas�n�z.
BeveledLabel=Inno Setup 5.1+ T�rk�e
BrowseDialogTitle=Dizine G�zat
BrowseDialogLabel=A�a��daki listeden bir dizin se�ip, daha sonra Tamam tu�una bas�n�z.
NewFolderName=Yeni Dizin

; *** "Welcome" wizard page
WelcomeLabel1=[name] Kurulum Sihirbaz�na Ho�geldiniz.
WelcomeLabel2=Kur �imdi [name/ver] program�n� bilgisayar�n�za y�kleyecektir.%n%nDevam etmeden �nce �al��an di�er b�t�n programlar� kapatman�z tavsiye edilir.

; *** "Password" wizard page
WizardPassword=�ifre
PasswordLabel1=Bu kurulum �ifre korumal�d�r.
PasswordLabel3=L�tfen �ifreyi giriniz. Daha sonra devam etmek i�in �leri'ye bas�n�z. L�tfen �ifreyi girerken B�y�k-K���k harflere dikkat ediniz.
PasswordEditLabel=&�ifre:
IncorrectPassword=Girdi�iniz �ifre hatal�. L�tfen tekrar deneyiniz.

; *** "License Agreement" wizard page
WizardLicense=Lisans S�zle�mesi
LicenseLabel=L�tfen devam etmeden �nce a�a��daki �nemli bilgileri okuyunuz.
LicenseLabel3=L�tfen A�a��daki Lisans S�zle�mesini okuyunuz. Kuruluma devam edebilmeniz i�in s�zle�me ko�ullar�n� kabul etmi� olmal�s�n�z.
LicenseAccepted=S�zle�meyi Kabul &Ediyorum.
LicenseNotAccepted=S�zle�meyi Kabul Et&miyorum.

; *** "Information" wizard pages
WizardInfoBefore=Bilgi
InfoBeforeLabel=L�tfen devam etmeden �nce a�a��daki �nemli bilgileri okuyunuz.
InfoBeforeClickLabel=Kur ile devam etmeye haz�r oldu�unuzda �leri'yi t�klay�n�z.
WizardInfoAfter=Bilgi
InfoAfterLabel=L�tfen devam etmeden �nce a�a��daki �nemli bilgileri okuyunuz.
InfoAfterClickLabel=Kur ile devam etmeye haz�r oldu�unuzda �leri'yi t�klay�n�z.

; *** "User Information" wizard page
WizardUserInfo=Kullan�c� Bilgileri
UserInfoDesc=L�tfen bilgilerinizi giriniz.
UserInfoName=K&ullan�c� Ad�:
UserInfoOrg=�i&rket:
UserInfoSerial=&Seri Numaras�:
UserInfoNameRequired=Bir isim girmelisiniz.

; *** "Select Destination Directory" wizard page
WizardSelectDir=Kurulum Yolu Se�imi
SelectDirDesc=[name] uygulamas� kurulum dizini.
SelectDirLabel3=[name] uygulamas� a�a��daki dizine kurulacakt�r.
SelectDirBrowseLabel=Devam etmek i�in �leri, ba�ka bir dizin se�mek istiyorsan�z G�zat tu�una bas�n�z.
DiskSpaceMBLabel=Bu uygulama en az [mb] MB s�r�c� alan� gerektirmektedir.
ToUNCPathname=Kur UNC tipindeki dizin yollar�na (�rn:\\yol vb.) kurulum yapamaz. E�er A� �zerinde kurulum yapmaya �al���yorsan�z. Bir a� s�r�c�s� tan�tman�z gerekir.
InvalidPath=S�r�c� ismi ile birlikte tam yolu girmelisiniz; �rne�in %nC:\APP%n%n veya bir UNC yolunu %n%n\\sunucu\payla��m%n%n �eklinde girmelisiniz.
InvalidDrive=Se�ti�iniz s�r�c� bulunamad� veya ula��lam�yor. L�tfen ba�ka bir s�r�c� se�iniz.
DiskSpaceWarningTitle=Yetersiz Disk Alan�
DiskSpaceWarning=Kur en az %1 KB kullan�labilir disk alan� gerektirmektedir. Ancak se�ili diskte %2 KB bo� alan bulunmaktad�r.%n%nYine de devam etmek istiyor musunuz?
DirNameTooLong=Dizin ad� veya yolu �ok uzun.
InvalidDirName=Dizin ad� ge�ersiz.
BadDirName32=Dizin ad� takip eden karakterlerden her hangi birini i�eremez:%n%n%1
DirExistsTitle=Dizin Bulundu
DirExists=Dizin:%n%n%1%n%n zaten var. Yine de bu dizine kurmak istedi�inizden emin misiniz?
DirDoesntExistTitle=Dizin Bulunamad�
DirDoesntExist=Dizin:%n%n%1%n%nbulunmamaktad�r. Bu dizini olu�turmak ister misiniz?

; *** "Select Components" wizard page
WizardSelectComponents=Bile�en Se�imi
SelectComponentsDesc=Hangi bile�enleri kurmak istiyorsunuz?
SelectComponentsLabel2=A�a��dan, kurulmas�n� istedi�iniz bile�enleri se�tikten sonra �leri tu�una bas�n�z.
FullInstallation=Tam Kurulum
; if possible don't translate 'Compact' as 'Minimal' (I mean 'Minimal' in your language)
CompactInstallation=Normal Kurulum
CustomInstallation=�zel Kurulum
NoUninstallWarningTitle=Mevcut Bile�enler
NoUninstallWarning=Kur a�a��daki bile�enlerin kurulu oldu�unu tespit etti:%n%n%1%n%nBu bile�enlerin se�imini kald�rmak bile�enleri silmeyecek.%n%nYine de devam etmek istiyor musunuz?
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceMBLabel=Se�ti�iniz bile�enler i�in en az [mb] MB s�r�c� alan� gerekmektedir.

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=Ek Se�enekler
SelectTasksDesc=Ek kurulum se�enekleri.
SelectTasksLabel2=[name] uygulamas� ile kullanmak istedi�iniz ek se�enekleri i�aretleyip �leri tu�una bas�n�z.

; *** "Ba�lat Men�s� Dizini Se�" sihirbaz sayfas�
WizardSelectProgramGroup=Ba�lat Men�s� Dizinini Se�iniz
SelectStartMenuFolderDesc=Kur program k�sayollar�n� nereye yerle�tirsin?
SelectStartMenuFolderLabel3=Kur program�n k�sayollar�n� a�a��daki Ba�lat Men�s� dizinine kuracak.
SelectStartMenuFolderBrowseLabel=Devam etmek i�in, �leri'ye bas�n�z. Ba�ka bir dizin se�mek istiyorsan�z, G�zat'a bas�n�z.
MustEnterGroupName=Bir dizin ismi girmelisiniz.
GroupNameTooLong=Dizin ad� veya yolu �ok uzun.
InvalidGroupName=Dizin ad� ge�ersiz.
BadGroupName=Dizin ad�, takip eden karakterlerden her hangi birini i�eremez:%n%n%1
NoProgramGroupCheck2=&Ba�lat men�s�nde k�sayol olu�turma

; *** "Ready to Install" wizard page
WizardReady=Kurulum Ba�l�yor
ReadyLabel1=[name] uygulamas� kurulumu i�in gerekli bilgiler al�nd�.
ReadyLabel2a=Y�klemeye devam etmek i�in Kur, ayarlar�n�z� kontrol etmek veya de�i�tirmek i�in Geri tu�una t�klay�n�z.
ReadyLabel2b=Kuruluma devam etmek i�in Kur tu�una t�klay�n�z.
ReadyMemoUserInfo=Kullan�c� bilgisi:
ReadyMemoDir=Hedef dizin:
ReadyMemoType=Kurulum bi�imi:
ReadyMemoComponents=Se�ili bile�enler:
ReadyMemoGroup=Ba�lat Men�s� :
ReadyMemoTasks=Ek se�enekler:

; *** "Kur Haz�lan�yor" sihirbaz sayfas�
WizardPreparing=Kurulum Haz�rlan�yor
PreparingDesc=Kur [name] program�n� bilgisayar�n�za kurmak i�in haz�rlan�yor.
PreviousInstallNotCompleted=Bir �nceki Kurulum/Kald�r program�na ait i�lem tamamlanmam��.�nceki kurulum i�leminin tamamlanmas� i�in bilgisayar�n�z� yeniden ba�latmal�s�n�z.%n%nBilgisayar�n�z tekrar ba�lad�ktan sonra,Kurulum'u tekrar �al��t�rarak [name] program�n� kurma i�lemine devam edebilirsiniz.
CannotContinue=Kur devam edemiyor. L�tfen �ptal'e t�klay�p ��k�n.

; *** "Kuruluyor" sihirbaz
WizardInstalling=Kurulum ger�ekle�tiriliyor
InstallingLabel=L�tfen [name] uygulamas� bilgisayar�n�za kurulurken bekleyiniz.

; *** "Setup Completed" wizard page
FinishedHeadingLabel=[name] Kurulumu Tamamland�
FinishedLabelNoIcons=[name] uygulamas� bilgisayar�n�za ba�ar�yla kuruldu.
FinishedLabel=[name] uygulamas� bilgisayar�n�za ba�ar�yla kuruldu. Uygulamay� y�klenen k�sayol tu�lar�na basarak �al��t�rabilirsiniz.
ClickFinish=Bu pencereyi kapatmak i�in Son tu�una basabilirsiniz.
FinishedRestartLabel=[name] program�n�n kurulumunu bitirmek i�in, Kur bilgisayar�n�z� yeniden ba�latacak. Bilgisayar�n�z yeniden ba�lat�ls�n m�?
FinishedRestartMessage=[name] kurulumunu bitirmek i�in, bilgisayar�n�z�n yeniden ba�lat�lmas� gerekmektedir. %n%nBiligisayar�n�z yeniden ba�lat�ls�n m�?
ShowReadmeCheck=Beni Oku dosyas�n� okumak istiyorum.
YesRadio=&Evet , bilgisayar yeniden ba�lat�ls�n.
NoRadio=&Hay�r, daha sonra elle ba�lat�ls�n.
; used for example as 'Run MyProg.exe'
RunEntryExec=%1 uygulamas�n� �al��t�r
; used for example as 'View Readme.txt'
RunEntryShellExec=%1 dosyas�n� g�r�nt�le

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=Bir Sonraki Diski Tak�n�z
SelectDiskLabel2=%1 numaral� diski tak�p, Tamam'� t�klay�n�z.%n%nE�er dosyalar ba�ka bir yerde bulunuyor ise do�ru yolu yaz�n�z veya G�zat'� t�klay�n�z.
PathLabel=&Yol:
FileNotInDir2=" %1 " adl� dosya " %2 " dizininde bulunamad�. L�tfen do�ru diski veya dosyay� se�iniz.
SelectDirectoryLabel=L�tfen sonraki diskin yerini belirleyiniz.

; *** Installation phase messages
SetupAborted=Kurulum tamamlanamad�.%n%nL�tfen problemi d�zeltiniz veya Kurulum'u tekrar �al��t�r�n�z.
EntryAbortRetryIgnore=Tekrar denemek i�in "Tekrar Dene" ye , yine de devam etmek i�in Yoksay'a , kurulumu iptal etmek i�in ise �ptal'e t�klay�n�z.

; *** Installation status messages
StatusCreateDirs=Dizinler olu�turuluyor...
StatusExtractFiles=Dosyalar ��kart�l�yor...
StatusCreateIcons=Program k�sayollar� olu�turuluyor...
StatusCreateIniEntries=INI girdileri olu�turuluyor...
StatusCreateRegistryEntries=Kay�t Defteri girdileri olu�turuluyor...
StatusRegisterFiles=Dosyalar sisteme kaydediliyor...
StatusSavingUninstall=Kald�r bilgileri kaydediliyor...
StatusRunProgram=Kurulum sonland�r�l�yor...
StatusRollback=De�i�iklikler geri al�n�yor...

; *** Misc. errors
ErrorInternal2=�� hata: %1
ErrorFunctionFailedNoCode=%1 ba�ar�s�z oldu.
ErrorFunctionFailed=%1 ba�ar�s�z oldu; kod  %2
ErrorFunctionFailedWithMessage=%1 ba�ar�s�z oldu ; kod  %2.%n%3
ErrorExecutingProgram=%1 adl� dosya �al��t�r�lamad�.

; *** Registry errors
ErrorRegOpenKey=A�a��daki Kay�t Defteri anahtar� a��l�rken hata olu�tu:%n%1\%2
ErrorRegCreateKey=A�a��daki Kay�t Defteri anahtar� olu�turulurken hata olu�tu:%n%1\%2
ErrorRegWriteKey=A�a��daki Kay�t Defteri anahtar�na yaz�l�rken hata olu�tu:%n%1\%2

; *** INI errors
ErrorIniEntry=" %1 " adl� dosyada INI girdisi yazma hatas�.

; *** File copying errors
FileAbortRetryIgnore=Yeniden denemek i�in "Yeniden Dene" ye, dosyay� atlamak i�in Yoksay'a (�nerilmez), Kurulumu iptal etmek i�in �ptal'e t�klay�n�z.
FileAbortRetryIgnore2=Yeniden denemek i�in "Yeniden Dene" ye , yine de devam etmek i�in Yoksay'a (�nerilmez), Kurulumu �ptal etmek i�in �ptal'e t�klay�n�z.
SourceIsCorrupted=Kaynak dosya bozulmu�
SourceDoesntExist=%1 adl� kaynak dosya bulunamad�.
ExistingFileReadOnly=Dosya Salt Okunur.%n%nSalt Okunur �zelli�ini kald�r�p yeniden denemek i�in Yeniden Dene'yi , dosyas� atlamak i�in Yoksay'� , Kurulumu iptal etmek i�in �ptal'i t�klay�n�z.
ErrorReadingExistingDest=Dosyay� okurken bir hata olu�tu :
FileExists=Dosya zaten var.%n%nKurulum'un �zerine yazmas�n� ister misiniz?
ExistingFileNewer=Zaten var olan dosya Kurulum'un y�klemek istedi�i dosyadan daha yeni. Var olan dosyay� saklaman�z �nerilir.%n%nVar olan dosya saklans�n m�?
ErrorChangingAttr=Zaten var olan dosyan�n �zelli�i de�i�tirilirken bir hata olu�tu:
ErrorCreatingTemp=Hedef dizinde dosya olu�turulurken bir hata olu�tu:
ErrorReadingSource=Kaynak dosya okunurken bir hata olu�tu:
ErrorCopying=Bir dosya kopyalan�rken bir hata olu�tu:
ErrorReplacingExistingFile=Zaten var olan dosya de�i�tirilirken bir hata olu�tu:
ErrorRestartReplace=RestartReplace ba�ar�s�z oldu:
ErrorRenamingTemp=Hedef dizinde bulunan dosyan�n ad� de�i�tirilirken hata oldu:
ErrorRegisterServer=%1 adl� DLL/OCX sisteme tan�t�lamad�.
ErrorRegSvr32Failed=RegSvr32 ��k�� hatas� %1 ile ba�ar�s�z oldu
ErrorRegisterTypeLib=%1 adl� tip k�t�phanesi (Type Library) sisteme tan�t�lamad�

; *** Post-installation errors
ErrorOpeningReadme=Beni Oku dosyas� a��l�rken hata olu�tu.
ErrorRestartingComputer=Kurulum bilgisayar� yeniden ba�latamad�. L�tfen kendiniz kapat�n�z.

; *** Uninstaller messages
UninstallNotFound=%1 adl� dosya bulunamad�. Kald�rma program� �al��t�r�lamad�.
UninstallOpenError="%1" dosyas� a��lam�yor. Kald�rma program� �al��t�r�lamad�.
UninstallUnsupportedVer=%1 adl� Kald�r bilgi dosyas� kald�rma program�n�n bu s�r�m� ile uyu�muyor. Kald�rma program� �al��t�r�lamad�.
UninstallUnknownEntry=Kald�r Bilgi dosyas�ndaki %1 adl� sat�r anla��lamad�
ConfirmUninstall=%1 ve bile�enlerini kald�rmak istedi�inizden emin misiniz?
UninstallOnlyOnWin64=Bu kurulum sadece 64-bit Windows'lardan kald�r�labilir.
OnlyAdminCanUninstall=Bu kurulum sadece y�netici yetkisine sahip kullan�c�lar taraf�ndan kald�rabilir.
UninstallStatusLabel=%1 uygulamas� bilgisayar�n�zdan kald�r�l�rken l�tfen bekleyiniz...
UninstalledAll=%1 uygulamas� bilgisayar�n�zdan ba�ar�yla kald�r�ld�.
UninstalledMost=%1 program�n�n kald�r�lma i�lemi sona erdi.%n%nBaz� bile�enler kald�r�lamad�. Bu dosyalar� kendiniz silebilirsiniz.
UninstalledAndNeedsRestart=%1 program�n�n kald�r�lmas� tamamland�, Bilgisayar�n�z� yeniden ba�latmal�s�n�z.%n%n�imdi yeniden ba�lat�ls�n m�?
UninstallDataCorrupted="%1" adl� dosya bozuk. . Kald�rma program� �al��t�r�lamad�.

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=Payla��lan Dosya Kald�r�ls�n M�?
ConfirmDeleteSharedFile2=Sistemde payla��lan baz� dosyalar�n art�k hi�bir program taraf�ndan kullan�lmad���n� belirtiyor. Kald�r bu payla��lan dosyalar� silsin mi?%n%n Bu dosya baz� programlar tafar�ndan kullan�l�yorsa ve silinmesini isterseniz, bu programalar d�zg�n �al��mayabilir. Emin de�ilseniz, Hay�r'a t�klay�n�z. Dosyan�n sisteminizde durmas� hi�bir zarar vermez.
SharedFileNameLabel=Dosya ad�:
SharedFileLocationLabel=Yol:
WizardUninstalling=Kald�rma Durumu
StatusUninstalling=%1 uygulamas� kald�r�l�yor...
[CustomMessages]

NameAndVersion=%1 %2 s�r�m�
AdditionalIcons=Ek simgeler:
CreateDesktopIcon=Masa�st� simg&esi olu�tur
CreateQuickLaunchIcon=H�zl� Ba�lat simgesi &olu�tur
ProgramOnTheWeb=%1 Web Sitesi
UninstallProgram=%1 Uygulamas�n� Kald�r
LaunchProgram=%1 Uygulamas�n� A�
AssocFileExtension=%2 dosya uzant�lar�n� %1 ile ili�kilendir
AssocingFileExtension=%2 dosya uzant�lar� %1 ile ili�kilendiriliyor...
