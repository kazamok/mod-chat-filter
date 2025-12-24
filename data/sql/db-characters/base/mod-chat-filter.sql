DROP TABLE IF EXISTS `mod-chat-filter`;
CREATE TABLE IF NOT EXISTS `mod-chat-filter` (
  `guid` int(10) unsigned NOT NULL DEFAULT '0',         -- 고유ID
  `string` VARCHAR(12) NOT NULL COLLATE 'utf8mb4_bin',  -- 텍스트
  `level` TINYINT UNSIGNED NOT NULL DEFAULT '0',        -- 등급 수위(0:경고, 1:중간, 3:퇴장)
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
