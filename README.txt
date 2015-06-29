Vorrausssetzungen:
* 64-Bit System
* Windows 7
* Microsoft Visual Studio 2010 (64-Bit Version)
* Subversion (z.B.: TortoiseSVN (http://tortoisesvn.net/) )
* Rechner hängt im RWTH-Netz (ggf. per VPN), damit der Datenbankserver erreichbar ist.

Installation von Komponenten:
* "Microsoft Windows SDK v7.1\setup.exe" installieren
  (Falls die Installation fehlschlägt, müssen ggf. erst alte Redistributal Packages für Visual Studio 2010 deinstalliert werden und
  der Rechner neugestartet werden. Dann sollte das "Microsoft Windows SDK v7.1" installierbar sein)
* "KinectSDK-v1.6-Setup.exe" installieren
* "KinectDeveloperToolkit-v1.6.0-Setup.exe" installieren
* "MicrosoftSpeechPlatformSDK.msi" installieren
* "OpenCV 2.4.4" nach "C:\Program Files" kopieren
* "mysql-connector-odbc-5.1.10-winx64.msi" installieren
* ODBC Datenbankverbindung erstellen unter "Systemsteuerung->Verwaltung->Datenquellen (ODBC) einrichten"
  * Hinzufügen von eintrag mit "MySQL ODBC 5.1 Driver"
    * Data Source Name: kinectshop2015
    * TCP/IP Server: kinectsrv.lfb.rwth-aachen.de
    * Port 3306
    * User: kinectshopClient
    * Password: lfb-student2015
    * Database: kinectshop2015
    * Testen mit "Test" und dann mit "OK" speichern

Programm das erste Mal kompilieren:
* Mit SVN "https://svn.lfb.rwth-aachen.de/kinectshop2015/subversion/" auschecken z.B. nach "d:\kinectshop"
* Die vorgefertigte Projektdateien für Visual Studio "https://svn.lfb.rwth-aachen.de/kinectshop2015/trac/attachment/wiki/WikiStart/ProjectTemplate.zip" laden und in das Verzeichnis "d:\kinectshop" entpacken.
* "D:\kinectshop\KinectShop.sln" öffnen
  * "Project->KinectShop Properties..." öffnen und für die Configuration "Debug" und "Release" folgende Änderungen vornehmen:
    * Falls die Installationspfade der SDK anders sind, müssen die Projektpfade unter
      "VC++ Directories" für includes und libs angepasst werden.
    * "Debugging->Environment" muss entsprechend der Installationspfade gesetzt werden:
      PATH=C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin;C:\Program Files\OpenCV 2.4.4\build\x64\vc10\bin;%PATH%
* Oben in der Leiste "Debug" oder "Release" UND "x64" wählen und mit dem Play Pfeil das Programm starten.