<?php

/*
 * The Account Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class AccountModel {
    public $db = null;
    public $rdb = null;
    private $config = null;
    private $table = 'account';
    private $column = ['username', 'password', 'spcode', 'company', 'address', 'concurrent', 'options', 'description'];

    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->rdb = Yaf\Registry::get('rdb');
    }

    public function get($id = null) {
        $id = intval($id);
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . $id;
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
        }

        return $result;
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

        if (!isset($data['company'])) {
            return false;
        }

        $company = new CompanyModel();
        if (!$company->isExist($data['company'])) {
            return false;
        }

        if (!isset($data['options'])) {
            $data['options'] = 0;
        }

        $data['concurrent'] = 0;

        if (count($data) == count($this->column)) {
            $sql = 'INSERT INTO ' . $this->table;
            $sql .= '(username, password, spcode, company, address, concurrent, options, description) ';
            $sql .= 'VALUES(:username, :password, :spcode, :company, :address, :concurrent, :options, :description)';
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
                        $this->rdb->hMSet('account.' . $result['username'], $result);
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

        if (isset($data['username'])) {
            unset($data['username']);
        }

        $column = $this->keyAssembly($data);

        if (count($data) > 0) {
            $sql = 'UPDATE ' . $this->table . ' SET ' . $column . ' WHERE id = :id';
            $sth = $this->db->prepare($sql);
            $sth->bindValue(':id', $id, PDO::PARAM_INT);

            foreach ($data as $key => $val) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }

            if ($sth->execute()) {
                $result = $this->get($id);
                if ($result != null) {
                    $this->rdb->hMSet('account.' . $result['username'], $result);
                    $this->signalNotification($result['id']);
                }

                return true;
            }
        }

        return false;
    }
    
    public function delete($id = null) {
        $id = intval($id);

        if ($this->checkDepend($id)) {
            $account = $this->get($id);
            $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . $id;
            if ($this->db->query($sql)) {
                $this->rdb->hSet('client.' . $id, 'signal', 9);
                $this->rdb->expire('client.' . $id, 30);
                $this->rdb->hSet('server.' . $id, 'signal', 9);
                $this->rdb->expire('server.' . $id, 30);
                if (isset($account['username'])) {
                    $this->rdb->del('account.' . $account['username']);
                }
                return true;
            }
        }

        return false;
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
            case 'address':
                $res['address'] = Filter::ip($val, null);
                break;
            case 'concurrent':
                $res['concurrent'] = Filter::number($val, 30, 1);
                break;
            case 'options':
                $res['options'] = Filter::number($val, 0, 0);
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

    public function checkDepend(int $id = 0) {
        if ($this->checkDepTemplet($id)) {
            if ($this->checkDepRouting($id)) {
                return true;
            }
        }

        return false;
    }
    
    public function checkDepTemplet(int $id = 0) {
        $sql = 'SELECT count(id) FROM template WHERE acc = :id';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);

        if ($sth->execute()) {
            $count = $sth->fetchColumn();
            if ($count < 1) {
                return true;
            }
        }

        return false;
    }

    public function checkDepRouting(int $id = 0) {
        $sql = 'SELECT count(id) FROM channels WHERE acc = :id';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);

        if ($sth->execute()) {
            $count = $sth->fetchColumn();
            if ($count < 1) {
                return true;
            }
        }

        return false;
    }

    public function signalNotification(int $id = 0) {
        if ($id > 0 && $this->isExist($id)) {
            $this->rdb->hSet('server.' . $id, 'signal', 1);
            return true;
        }

        return false;
    }
}
