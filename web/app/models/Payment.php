<?php

/*
 * The Payment Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class PaymentModel {
    public $db = null;
    private $table = 'pay_logs';
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function getLogs() {
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id DESC';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }
    
    public function writeLogs(array $event) {
        $sql = 'INSERT INTO ' . $this->table . '(company, money, operator, ip_addr) VALUES(:company, :money, :operator, :ip_addr)';
        $sth = $this->db->prepare($sql);
        foreach ($event as $key => $val) {
            $sth->bindParam(':' . $key, $event[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
        }

        if ($sth->execute()) {
            return true;
        }

        return false;
    }
}
