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
    public $rdb = null;
    public $rdk = null;
    public $config = null;
    private $table = 'company';
    private $column = ['name', 'contacts', 'telephone', 'description'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->rdb = Yaf\Registry::get('rdb');
        $this->config = Yaf\Registry::get('config');

        if ($this->config) {
            $cfg = $this->config->backup;
            $rdk = new Redis($cfg->host, $cfg->port, $cfg->password, $cfg->db);
            $this->rdk = $rdk->handle;
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
            foreach ($result as &$val) {
                $val['money'] = $this->getMoney($val['id']);
            }
        }

        return $result;
    }

    public function create(array $data) {
        $data = $this->checkArgs($data);
        
        if (count($data) == count($this->column)) {
            $column = 'name, contacts, telephone, description';
            $sql = 'INSERT INTO ' . $this->table . '(' . $column . ') VALUES(:name, :contacts, :telephone, :description)';
            $sth = $this->db->prepare($sql);

            foreach ($data as $key => $val) {
                if ($val !== null) {
                    $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
                } else {
                    return false;
                }

            }

            if ($sth->execute()) {
                $this->upCache();
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
            $sth->bindValue(':id', $id, PDO::PARAM_INT);
            
            foreach ($data as $key => $val) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }

            if ($sth->execute()) {
                return true;
            }
        }

        return false;
    }
    
    public function delete($id = null) {
        $id = intval($id);
        if (!$this->isUsed($id)) {
            $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . $id;
            if ($this->db->query($sql)) {
                return true;
            }
        }

        return false;
    }

    public function accountTotal($id = null) {
        $count = 0;
        $id = intval($id);

        $sql = 'SELECT count(id) FROM account WHERE company = :id';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);

        if ($sth->execute()) {
            $count = intval($sth->fetchColumn());
        }

        return $count;
    }

    public function isUsed($id = null) {
        $id = intval($id);
        $sql = 'SELECT count(id) FROM account WHERE company = :id';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);

        if ($sth->execute()) {
            $count = $sth->fetchColumn();
            if ($count > 0) {
                return true;
            }
        }

        return false;
    }
    
    public function recharge($id = 0, $money = 0, $description = null): bool {
        $id = intval($id);

        if ($money == 0) {
            return true;
        }

        if (!$this->isExist($id)) {
            return false;
        }
        
        if (!is_string($description) || strlen($description) < 1) {
            $description = 'no description';
        }

        $this->rdb->multi()->hIncrBy('company.' . $id, 'money', $money)->exec();
        $payment = new PaymentModel();
        $company = $this->get($id);
        $event['company'] = $company['name'];
        $event['money'] = $money;
        $event['operator'] = 'admin';
        $event['description'] = $description;
        $event['ip_addr'] = ip2long($_SERVER['REMOTE_ADDR']);
        $payment->writeLogs($event);

        return true;
    }

    public function checkArgs(array $data) {
        $res = array();
        $data = array_intersect_key($data, array_flip($this->column));

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'name':
                $res['name'] = Filter::string($val, null, 1, 32);
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
        $id = intval($id);
        $sql = 'SELECT count(id) FROM ' . $this->table . ' WHERE id = :id LIMIT 1';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);

        if ($sth->execute()) {
            $count = $sth->fetchColumn();
            if ($count > 0) {
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
        $id = intval($id);
        $result = $this->rdk->hGet('company.' . $id, 'money');
        if ($result !== false) {
            $money = intval($result);
        }

        return $money;
    }

    public function upCache() {
        $sql = 'SELECT * FROM company';
        $sth = $this->db->query($sql);

        if ($sth) {
            $companys = $sth->fetchAll();
            foreach ($companys as $company) {
                unset($company['money']);
                $this->rdb->hMSet('company.' . $company['id'], $company);
            }
        }

        return true;
    }
}
