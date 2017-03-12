<?php
function check_cc($cc, $extra_check = false)
{
   $cards = array
   (
      "visa" => "(4\d{12}(?:\d{3})?)",
      "amex" => "(3[47]\d{13})",
      "jcb" => "(35[2-8][89]\d\d\d{10})",
      "maestro" => "((?:5020|5038|6304|6579|6761)\d{12}(?:\d\d)?)",
      "solo" => "((?:6334|6767)\d{12}(?:\d\d)?\d?)",
      "mastercard" => "(5[1-5]\d{14})",
      "switch" => "(?:(?:(?:4903|4905|4911|4936|6333|6759)\d{12})|(?:(?:564182|633110)\d{10})(\d\d)?\d?)",
   );
   $names   = array("Visa", "American Express", "JCB", "Maestro", "Solo", "Mastercard", "Switch");
   $matches = array();
   $pattern = "#^(?:".implode("|", $cards).")$#";
   $result  = preg_match($pattern, str_replace(" ", "", $cc), $matches);
   if($extra_check && $result > 0)
      $result = (validatecard($cc)) ? 1 : 0;
   return ($result > 0) ? $names[sizeof($matches) - 2] : false;
}

function is_valid_luhn($number)
{
   settype($number, 'string');
   $sumTable = array
   (
      array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9),
      array(0, 2, 4, 6, 8, 1, 3, 5, 7, 9)
   );
   $sum  = 0;
   $flip = 0;
   for($i = strlen($number) - 1; $i >= 0; $i--)
      $sum += $sumTable[$flip++ & 0x1][$number[$i]];
   return $sum % 10 === 0;
}

function is_valid_card($str)
{
   $strLen = strlen($str);
   if($strLen >= 13 && $strLen <= 19)
   {
      if(is_valid_luhn($str) && check_cc($str) !== false)
         return true;
   }
   return false;
}

function found_card($str)
{
   $str     = strstr($str, "\r\n\r\n");
   $currNum = '';
   for($i = 0; $i < strlen($str); ++$i)
   {
      if(ctype_digit($str[$i]))
         $currNum .= $str[$i];
      else if($str[$i] != '+')
      {
         if(is_valid_card($currNum))
            return true;
         $currNum = '';
      }
   }
   if(is_valid_card($currNum))
      return true;
   return false;
}
?>