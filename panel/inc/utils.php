<?php
function escape_php_string($str)
{
   $str = str_replace("\\", "\\\\", $str);
   $str = str_replace("\"", "\\\"", $str);
   $str = str_replace("\'", "\\\'", $str);
   $str = str_replace("\n", "\\n", $str); 
   $str = str_replace("\t", "\\t", $str);   
   $str = str_replace("\r", "\\r", $str); 
   $str = str_replace("$", "\\$", $str);
   return $str;
}

function hash_pass($pass)
{
   return hash('sha512', $pass);
}

function set_headers_txt()
{
   header('X-Content-Type-Options: nosniff'); //stop chrome from downloading the file
   header('Content-Type: text/plain');
}

function echo_file_upload_error()
{
   echo('<div class="error">No file uploaded</div>');
}

function gen_qmarks($arr)
{
   return str_repeat('?, ', count($arr) - 1).'?';
}

function get_pag_vars($total, &$pages, &$page, &$offset)
{
   global $CONST_PAGE_LIMIT;
   $pages = ceil($total / $CONST_PAGE_LIMIT);
   $page = 1;
   if(isset($_GET['page']))
   {
      $page = (int) $_GET['page'];
      if($page > $pages)
         $page = $pages;
      else if($page < 1)
         $page = 1;
   }
   $offset = ($page - 1) * $CONST_PAGE_LIMIT;
}

function get_os($majorVer, $minorVer, $server)
{
   if($majorVer == 5)
   {
     if(!$server)
         return 'Windows XP';
      else
        return 'Windows 2003';
   }
   if($majorVer == 6 && $minorVer == 0)
   {
     if(!$server)
         return 'Windows Vista';
      else
        return 'Windows Server 2008';
   }
   if($majorVer == 6 && $minorVer == 1)
   {
     if(!$server)
         return 'Windows 7';
     else
        return 'Windows Server 2008 R2';
   }
   if($majorVer == 6 && $minorVer == 2)
   {
     if(!$server)
         return 'Windows 8';
     else
        return 'Windows Server 2012';
   }
   if($majorVer == 6 && $minorVer == 3)
   {
     if(!$server)
         return 'Windows 8.1';
     else
        return 'Windows Server 2012 R2';
   }
   if($majorVer == 10 && $minorVer == 0)
   {
      if(!$server)
         return 'Windows 10';
      else
         return 'Windows Server 2016';
   }
   else
     return '?';
}

function format_time($time)
{
   return date('d/m/Y H:i:s', $time);  
}

function time_since($time)
{
   $time = time() - $time;
   $time = ($time < 1) ? 1 : $time;
   $tokens = array (
      31536000 => 'year',
      2592000  => 'month',
      604800   => 'week',
      86400    => 'day',
      3600     => 'hour',
      60       => 'minute',
      1        => 'second'
   );

   foreach($tokens as $unit => $text)
   {
      if($time < $unit) continue;
      $numberOfUnits = floor($time / $unit);
      return $numberOfUnits.' '.$text.(($numberOfUnits > 1) ? 's' : '').' ago';
   }
}

function is_online($time)
{
   global $CONF_TIMEOUT_OFFLINE;
   return (time() - $time) < $CONF_TIMEOUT_OFFLINE ;
}

function echo_hidden_fields()
{
   $args = func_get_args();
   foreach($_GET as $name => $value)
   {
      if(!in_array($name, $args))
         echo('<input type="hidden" name="'.$name.'" value="'.$value.'">');
   }
}

function echo_pag_form($page, $pages)
{
   $firstDisabled = $page == 1 ? 'disabled' : '';
   echo('<form method="GET" class="margin-top"><a class="btn '.$firstDisabled.'" href="'.add_get_param('page', 1).'">First</a>');
   echo(' <a class="btn '.$firstDisabled.'" href="'.add_get_param('page', $page - 1).'">Previous</a>');
   echo_hidden_fields('page');
   echo(' <input type="text" name="page" placeholder="'.$page.' / '.$pages.'" style="width: 70px; text-align: center;" 
      class="'.($pages == 1 ? 'disabled' : '').' input">');
   $lastDisabled = $page == $pages ? 'disabled' : '';
   echo(' <a class="btn '.$lastDisabled.'" href="'.add_get_param('page', $page + 1).'">Next</a>');
   echo(' <a class="btn '.$lastDisabled.'" href="'.add_get_param('page', $pages).'">Last</a></form>');    
}

function add_get_param($name, $value)
{
   $params = $_GET;
   unset($params[$name]);
   $params[$name] = $value;
   return basename($_SERVER['PHP_SELF']).'?'.http_build_query($params);
}

function action_sec_check()
{
   if($_SERVER['REQUEST_METHOD'] == 'POST')
      $userTime = $_POST['time'];
   else
      $userTime = $_GET['time'];
   if($userTime != $_SESSION['time'])
      exit();
}
?>