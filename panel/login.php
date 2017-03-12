<?php
session_start();
require_once('settings_data.php');
require_once('inc/utils.php');
if(isset($_SESSION['auth']))
{
   header('location: index.php');
   exit();
}

function set_bad_login_headers()
{
   header('WWW-Authenticate: Basic realm=""');
   header('HTTP/1.0 401 Unauthorized');
}

if(!isset($_SERVER['PHP_AUTH_USER']))
   set_bad_login_headers();
else
{
    if($_SERVER['PHP_AUTH_USER']          === $CONF_PANEL_USER && 
       hash_pass($_SERVER['PHP_AUTH_PW']) === $CONF_PANEL_PASS)
    {
       $_SESSION['auth'] = true;
       $_SESSION['time'] = (string) microtime(true);
       header('location: index.php');
       exit();
    }
    else
       set_bad_login_headers();
}
echo('<meta http-equiv="refresh" content="0; url=login.php">');
?>