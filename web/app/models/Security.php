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
        $result = [];
        $sql = 'SELECT * FROM blacklist LIMIT 32';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function whitelist() {
        $result = [];
        $sql = 'SELECT * FROM whitelist LIMIT 32';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function total($type = null) {
        $count = 0;
        if (in_array($type, ['blacklist', 'whitelist'], true)) {
            $sql = 'SELECT count(phone) FROM ' . $type;
            $sth = $this->db->query($sql);
            if ($sth) {
                $result = $sth->fetch();
                $count = $result['count'];
            }
        }

        return $count;
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
            $result = false;
            $phone = intval($phone);
            $table = ($type === 'blacklist') ? 'blacklist' : 'whitelist';
            $sql = 'SELECT count(phone) FROM ' . $table . ' WHERE phone = ' . $phone;
            $sth = $this->db->query($sql);
            if ($sth && ($result = $sth->fetch()) !== false) {
                if ($result['count'] > 0) {
                    return true;
                }
            }
        }
        
        return false;
    }
}
