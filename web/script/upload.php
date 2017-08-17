#!/usr/local/php-7.1/bin/php
<?php

set_time_limit(3600);

define('HOST', '127.0.0.1');
define('PORT', 5432);
define('USER', 'postgres');
define('PASSWORD', 'postgres');
define('DBNAME', 'lamb');
    
if (php_sapi_name() != 'cli') {
    exit(0);
}

try {
    if (!isset($argv[1], $argv[2])) {
        exit(0);
    }

    $type = $argv[1];
    $file = $argv[2];

    if (!in_array($type, ['blacklist', 'whitelist'], true)) {
        exit(0);
    }
    
    $info = 'pgsql:host=' . HOST . ';port=' . PORT . ';dbname=' . DBNAME;
    $db = new PDO($info, USER, PASSWORD);

    if ($db && file_exists($file) && is_readable($file)) {
        $handle = fopen($file, 'r');
        if ($handle) {
            $phone = false;
            while (($phone = fgets($handle, 12)) !== false) {
                if (strlen($phone) === 11) {
                    $sql = 'INSERT INTO ' . $type . ' VALUES(' . intval($phone) . ')';
                    $db->query($sql);
                }
            }
            fclose($handle);
        }

        $db = null;
        unlink($file);
        exit(0);
    }
    error_log("can't open upload file", 0);
} catch (PDOException $e) {
    error_log(0, $e->getMessage());
}

