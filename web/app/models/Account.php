<?php

/*
 * The Account Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Db\Redis;
use Tool\Filter;

class AccountModel {
    public $db = null;
    public $redis = null;
    private $config = null;
    private $table = 'account';
    private $column = ['username', 'password', 'spcode', 'company', 'charge_type', 'ip_addr', 'concurrent', 'route', 'extended', 'policy', 'check_template', 'check_keyword', 'description'];

    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->config = Yaf\Registry::get('config');
        $config = $this->config->redis;
        $redis = new Redis($config->host, $config->port, $config->password, $config->db);
        $this->redis = $redis->handle;
    }

    public function get($id = null) {
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($id);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if ($result !== false) {
                return $result;
            }
        }

        return null;
    }
    
    public function getAll() {
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY company, create_time';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function create(array $data = null) {
        $data = $this->checkArgs($data);

        if (isset($data['company'])) {
            $company = new CompanyModel();
            if (!$company->isExist($data['company'])) {
                return false;
            }
        } else {
            return false;
        }
        
        if (!isset($data['extended'])) {
            $data['extended'] = 0;
        }

        if (!isset($data['policy'])) {
            $data['policy'] = 0;
        }

        if (!isset($data['check_template'])) {
            $data['check_template'] = 0;
        }

        if (!isset($data['check_keyword'])) {
            $data['check_keyword'] = 0;
        }

        if (!isset($data['route'])) {
            return false;
        }

        $group = new GroupModel();
        if (!$group->isExist($data['route'])) {
            return false;
        }
        
        if (count($data) == count($this->column)) {
            $sql = 'INSERT INTO ' . $this->table;
            $sql .= '(username, password, spcode, company, charge_type, ip_addr, concurrent, route, extended, policy, check_template, check_keyword, description) ';
            $sql .= 'VALUES(:username, :password, :spcode, :company, :charge_type, :ip_addr, :concurrent, :route, :extended, :policy, :check_template, :check_keyword, :description)';
            $sth = $this->db->prepare($sql);

            foreach ($data as $key => $val) {
                if ($val !== null) {
                    $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
                } else {
                    return false;
                }
            }

            if ($sth->execute()) {
                $sql = 'SELECT * FROM account WHERE username = :username';
                $sth = $this->db->prepare($sql);
                $sth->bindValue(':username', $data['username'], PDO::PARAM_STR);
                if ($sth->execute()) {
                    $result = $sth->fetch();
                    if ($result !== false) {
                        $this->redis->hMSet('account.' . $result['username'], $data);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    public function change($id = null, array $data) {
        $id = intval($id);
        $data = $this->checkArgs($data);

        if (isset($data['company'])) {
            $company = new CompanyModel();
            if (!$company->isExist($data['company'])) {
                return false;
            }
        }

        if (!isset($data['extended'])) {
            $data['extended'] = 0;
        }

        if (!isset($data['policy'])) {
            $data['policy'] = 0;
        }

        if (!isset($data['check_template'])) {
            $data['check_template'] = 0;
        }

        if (!isset($data['check_keyword'])) {
            $data['check_keyword'] = 0;
        }
        
        $column = $this->keyAssembly($data);

        if (count($data) > 0) {
            $sql = 'UPDATE ' . $this->table . ' SET ' . $column . ' WHERE id = :id';
            $sth = $this->db->prepare($sql);
            $sth->bindParam(':id', $id, PDO::PARAM_INT);

            foreach ($data as $key => $val) {
                $sth->bindParam(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }

            if ($sth->execute()) {
                $result = $this->get($id);
                if ($result != null) {
                    $this->redis->hMSet('account.' . $result['username'], $result);
                }
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

    public function checkArgs(array $data) {
        $res = array();
        $data = array_intersect_key($data, array_flip($this->column));

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'username':
                $res['username'] = Filter::string($val, null, 6, 6);
                break;
            case 'password':
                $res['password'] = Filter::alpha($val, null, 6, 64);
                break;
            case 'spcode':
                $res['spcode'] = Filter::alpha($val, null, 1, 20);
                break;
            case 'company':
                $res['company'] = Filter::number($val, null, 1);
                break;
            case 'charge_type':
                $res['charge_type'] = Filter::number($val, 1, 1, 2);
                break;
            case 'ip_addr':
                $res['ip_addr'] = Filter::ip($val, null);
                break;
            case 'concurrent':
                $res['concurrent'] = Filter::number($val, 30, 1);
                break;
            case 'route':
                $res['route'] = Filter::number($val, null, 1);
                break;
            case 'extended':
                $res['extended'] = Filter::number($val, null, 0, 1);
                break;
            case 'policy':
                $res['policy'] = Filter::number($val, null, 1, 2);
                break;
            case 'check_template':
                $res['check_template'] = Filter::number($val, null, 0, 1);
                break;
            case 'check_keyword':
                $res['check_keyword'] = Filter::number($val, null, 0, 1);
                break;
            case 'description':
                $res['description'] = Filter::string($val, 'no description', 1, 64);
                break;
            }
        }

        return $res;
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
}
