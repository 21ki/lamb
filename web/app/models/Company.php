<?php

/*
 * The Company Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Db\Redis;
use Tool\Filter;

class CompanyModel {
    public $db = null;
    public $redis = null;
    public $backup = null;
    public $config = null;
    private $table = 'company';
    private $column = ['name', 'paytype', 'contacts', 'telephone', 'description'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->config = Yaf\Registry::get('config');
        if ($this->config) {
            $rcfg = $this->config->redis;
            $bcfg = $this->config->backup;
            $redis = new Redis($rcfg->host, $rcfg->port, $rcfg->password, $rcfg->db);
            $this->redis = $redis->handle;
            $backup = new Redis($bcfg->host, $bcfg->port, $bcfg->password, $bcfg->db);
            $this->backup = $backup->handle;
        }
    }

    public function get($id = null) {
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($id) . ' LIMIT 1';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if ($result !== false) {
                return $result;
            }
        }

        return null;
    }

    public function getAll(array $column = null) {
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function create(array $data) {
        $data = $this->checkArgs($data);
        
        if (count($data) == count($this->column)) {
            $sql = 'INSERT INTO ' . $this->table . '(name, paytype, contacts, telephone, description) VALUES(:name, :paytype, :contacts, :telephone, :description)';
            $sth = $this->db->prepare($sql);

            foreach ($data as $key => $val) {
                if ($val !== null) {
                    $sth->bindParam(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
                } else {
                    return false;
                }

            }

            if ($sth->execute()) {
                return true;
            }
        }

        return false;
    }

    public function change($id = null, array $data = null) {
        $id = intval($id);
        $data = $this->checkArgs($data);
        $column = $this->keyAssembly($data);

        if (count($data) > 0) {
            $sql = 'UPDATE ' . $this->table . ' SET ' . $column . ' WHERE id = :id';
            $sth = $this->db->prepare($sql);
            $sth->bindParam(':id', $id, PDO::PARAM_INT);
            
            foreach ($data as $key => $val) {
                $sth->bindParam(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }

            if ($sth->execute()) {
                return true;
            }
        }

        return false;
    }
    
    public function delete($id = null) {
        $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . intval($id);
        if ($this->db->query($sql)) {
            return true;
        }

        return false;
    }

    public function accountTotal($id = null) {
        $count = 0;
        $sql = 'SELECT count(id) FROM account WHERE company = ' . intval($id);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if ($result !== false) {
                $count = $result['count'];
            }
        }

        return $count;
    }
    
    public function recharge($id = null, $money = 0) {
        $id = intval($id);
        $money = intval($money);
        if ($money !== 0 && $this->isExist($id)) {
            $this->redis->multi()->hIncrBy('company.' . $id, 'money', $money)->exec();
            $payment = new PaymentModel();
            $company = $this->get($id);
            $event['company'] = $company['name'];
            $event['money'] = intval($money);
            $event['operator'] = 'admin';
            $event['description'] = 'no description';
            $event['ip_addr'] = ip2long($_SERVER['REMOTE_ADDR']);
            $payment->writeLogs($event);
            return true;
        }

        return false;
    }

    public function checkArgs(array $data) {
        $res = array();
        $data = array_intersect_key($data, array_flip($this->column));

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'name':
                $res['name'] = Filter::string($val, null, 1, 32);
                break;
            case 'paytype':
                $res['paytype'] = Filter::number($val, 1, 1, 2);
                break;
            case 'contacts':
                $res['contacts'] = Filter::string($val, null, 1, 32);
                break;
            case 'telephone':
                $res['telephone'] = Filter::alpha($val, null, 1, 21);
                break;
            case 'description':
                $res['description'] = Filter::string($val, 'no description', 1, 64);
                break;
            }
        }

        return $res;
    }

    public function isExist($id = null) {
        $result = false;
        $sql = 'SELECT count(id) FROM ' . $this->table . ' WHERE id = ' . intval($id) . ' LIMIT 1';
        $sth = $this->db->query($sql);
        if ($sth && ($result = $sth->fetch()) !== false) {
            if ($result['count'] > 0) {
                return true;
            }
        }

        return false;
    }

    public function keyAssembly(array $data) {
    	$text = '';
        $append = false;
        foreach ($data as $key => $val) {
            if ($val !== null) {
                if ($text != '' && $append) {
                    $text .= ", $key = :$key";
                } else {
                    $append = true;
                    $text .= "$key = :$key";
                }
            }
        }
        return $text;
    }

    public function getMoney($id = null) {
        $money = 0;
        $result = $this->backup->hGet('company.' . intval($id), 'money');
        if ($result !== false) {
            $money = intval($result);
        }

        return $money;
    }
}
