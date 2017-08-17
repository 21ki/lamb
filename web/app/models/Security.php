<?php

/*
 * The Security Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class SecurityModel {
    public $db = null;
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function blacklist() {
        $sql = 'SELECT * FROM blacklist';
        $result = $this->db->query($sql)->fetchAll();
        return $result;
    }

    public function whitelist() {
        $sql = 'SELECT * FROM whitelist';
        $result = $this->db->query($sql)->fetchAll();
        return $result;
    }

    public function delete($type = null, $phone = null) {
        if (in_array($type, ['blacklist', 'whitelist'], true)) {
            $sql = 'DELETE FROM ' . $type . ' WHERE phone = ' . intval($phone);
            if ($this->db->query($sql)) {
                return true;
            }
        }

        return false;
    }

    public function import($type = null, array $file = null) {
        if (in_array($type, ['blacklist', 'whitelist'], true)) {
            $tmp = isset($file['tmp_name']) ? $file['tmp_name'] : null;
            $finfo = finfo_open(FILEINFO_MIME);
            $mimetype = finfo_file($finfo, $tmp, FILEINFO_MIME_TYPE);
            finfo_close($finfo);

            if ($mimetype === 'text/plain') {
                $file = $tmp . '.dat';
                if (move_uploaded_file($tmp, $file)) {
                    $prog = APP_PATH . '/script/upload.php';
                    switch ($type) {
                    case 'blacklist':
                        system($prog . ' blacklist ' . $file);
                        break;
                    case 'whitelist':
                        system($prog . ' whitelist ' . $file);
                        break;
                    default:
                        unlink($file);
                        break;
                    }

                    return true;
                }
            }

            if (is_uploaded_file($tmp)) {
                unlink($tmp);
            }
        }

        return false;
    }

    public function isExist($type = null, $phone = null) {
        if (in_array($type, ['blacklist', 'whitelist'], true)) {
            $phone = intval($phone);
            $table = ($type === 'blacklist') ? 'blacklist' : 'whitelist';
            $sql = 'SELECT * FROM ' . $table . ' WHERE phone = ' . $phone;
            $sth = $this->db->query($sql);
            if ($sth && $sth->fetch()) {
                return true;
            }
        }
        
        return false;
    }
}
