using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutoEncrypt
{
   class Program
   {
      private static String EncStr(String str, String key)
      {
         String enc = "";
	 for(int i = 0; i < str.Length; ++i)
	 {
            int code = str[i] ^ key[i % key.Length];
            enc += "\\x" + code.ToString("X4");
	 } 
         return enc;  
      }

      private static Random random = new Random();
      private static string RandomString(int length)
      {
         const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
         return new string(Enumerable.Repeat(chars, length)
            .Select(s => s[random.Next(s.Length)]).ToArray());
      }

      static void Main(string[] args)
      {
         try
         {
            const String startEncA = "ENC_STR_A";
            const String endEnc    = "END_ENC_STR";
            const int    maxKeyLen = 128;
            String source          = File.ReadAllText(args[0]);
            File.WriteAllText(args[0], "");
            StreamWriter writer    = File.AppendText(args[0]);
            for(;;)
            {
               int indexStart;
               if((indexStart = source.IndexOf(startEncA)) == -1)
                  break;

               writer.Write(source.Substring(0, indexStart));

               int indexEnd;
               if((indexEnd = source.IndexOf(endEnc)) == -1)
                  break;

               String str2enc = source.Substring(indexStart + startEncA.Length + 1, indexEnd - indexStart - endEnc.Length);
               Console.WriteLine(str2enc);

               str2enc = System.Text.RegularExpressions.Regex.Unescape(str2enc);
               int len = str2enc.Length;

               source = source.Substring(indexEnd + endEnc.Length);

               String key = RandomString((str2enc.Length < maxKeyLen) ? str2enc.Length : maxKeyLen);
               str2enc = EncStr(str2enc, key);
               
               writer.Write("UnEnc(\"" + str2enc + "\", \"" + key + "\", " + len + ")");     
            }
            writer.Write(source);
            writer.Close();
         }
         catch(Exception e)
         {
            Console.WriteLine(e.ToString());
         }
         Console.ReadKey();
      }
   }
}