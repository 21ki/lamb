<?php

/*
 * The Company Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class CompanyModel {
    public $db = null;
    public $config = null;
    private $table = 'company';

    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->config = Yaf\Registry::get('config');
    }

    public function get($id = null) {
        $id = intval($id);
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = :id LIMIT 1';
        $sth = $this->db->prepare($sql);
        $sth->bindParam(':id', $id, PDO::PARAM_INT);
        $sth->execute();
        return $sth->fetch();
    }

    public function getAll() {
        $sql = 'SELECT * FROM ' . $this->table;
        $result = $this->db->query($sql)->fetchAll();
        return $result;
    }
}
