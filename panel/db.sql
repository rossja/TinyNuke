/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!50503 SET NAMES utf8mb4 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

DROP DATABASE IF EXISTS `panel`;
CREATE DATABASE IF NOT EXISTS `panel` /*!40100 DEFAULT CHARACTER SET latin1 */;
USE `panel`;

DROP TABLE IF EXISTS `bots`;
CREATE TABLE IF NOT EXISTS `bots` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `uhid` varchar(50) NOT NULL,
  `os_major` tinyint(4) unsigned NOT NULL,
  `os_minor` tinyint(4) unsigned NOT NULL,
  `service_pack` tinyint(4) unsigned NOT NULL,
  `is_server` tinyint(4) unsigned NOT NULL,
  `comp_name` varchar(50) NOT NULL,
  `user_name` varchar(50) NOT NULL,
  `ip` int(11) unsigned NOT NULL,
  `is_x64` tinyint(4) unsigned NOT NULL,
  `last_seen` int(11) unsigned NOT NULL,
  `first_seen` int(11) unsigned NOT NULL,
  `country` char(2) NOT NULL,
  `last_command` int(11) unsigned NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uhid` (`uhid`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS `commands`;
CREATE TABLE IF NOT EXISTS `commands` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `execs` int(11) unsigned NOT NULL,
  `limit` int(11) unsigned NOT NULL,
  `enabled` tinyint(4) unsigned NOT NULL,
  `created` int(11) unsigned NOT NULL,
  `type` tinyint(4) unsigned NOT NULL,
  `param` text NOT NULL,
  `countries` text NOT NULL,
  `uhids` text NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

DROP TABLE IF EXISTS `reports`;
CREATE TABLE IF NOT EXISTS `reports` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `uhid` varchar(50) NOT NULL,
  `software` varchar(20) NOT NULL,
  `url` varchar(100) NOT NULL,
  `received` int(11) unsigned NOT NULL,
  `content` text NOT NULL,
  `inject` tinyint(4) unsigned NOT NULL,
  `found_card` tinyint(4) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
