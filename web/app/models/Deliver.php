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
        try {
            $this->config = Yaf\Registry::get('config');
            $config = $this->config->msg;
            $options = [PDO::ATTR_PERSISTENT => true];
            $this->db = new PDO('pgsql:host=' . $config->host . ';port=' . $config->port . ';dbname=' . $config->name, $config->user, $config->pass, $options);
            $this->db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
        } catch (PDOException $e) {
            error_log($e->getMessage(), 0);
        }
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
