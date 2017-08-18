<?php

/*
 * The Deliver Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class DeliverModel {
    public $db = null;
    private $table = 'deliver';
    
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
}
