<?php

/*
 * The Keyword Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class KeywordModel {
    public $db = null;
    private $table = 'keyword';
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function getTags() {
        $reply = [];
        $sql = 'SELECT tag FROM ' . $this->table . ' GROUP BY tag';
        $sth = $this->db->query($sql);
        if ($sth) {
            $reply = $sth->fetchAll();
        }

        return $reply;
    }
}
