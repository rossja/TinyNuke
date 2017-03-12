<?php
function db($message = true)
{
  global $CONF_DB_HOST, $CONF_DB_NAME, $CONF_DB_USER, $CONF_DB_PASS;
  try
  {
     return new PDO('mysql:host='.$CONF_DB_HOST.';dbname='.$CONF_DB_NAME.';charset=utf8', $CONF_DB_USER, $CONF_DB_PASS, 
        array(PDO::MYSQL_ATTR_INIT_COMMAND => 'SET NAMES "utf8"'));
  }
  catch(PDOException $e)
  {
     if($message)
        echo 'Can\'t connect to the database. Change <a href="settings.php">settings</a>?';
     exit();
  }
}
?>