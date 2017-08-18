<?php

/*
 * The Report Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class ReportModel {
    public $db = null;
    private $table = 'report';
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function getAll() {
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY create_time DESC LIMIT 32';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

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
