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
}
