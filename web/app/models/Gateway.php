<?php

/*
 * The Channel Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class GatewayModel {
    public $db = null;
    private $table = 'gateway';
    private $column = ['name', 'type', 'host', 'port', 'username', 'password', 'spid', 'spcode', 'encoded', 'extended', 'service', 'concurrent', 'description'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
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
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function create(array $data = null) {
        $data = $this->checkArgs($data);
        $data['type'] = 1;

        if (isset($data['encoded'])) {
            if (!in_array($data['encoded'], [0, 8, 15], true)) {
                return false;
            }
        }

        if (isset($data['extended'])) {
            $data['extended'] = 1;
        } else {
            $data['extended'] = 0;
        }

        if (isset($data['service'])) {
            $data['service'] = '{' . $data['service'] . '}';
        } else {
            $data['service'] = '{0}';
        }

        if (count($data) == count($this->column)) {
            $sql = 'INSERT INTO ' . $this->table . '(name, type, host, port, username, password, spid, spcode, encoded, extended, service, concurrent, description) ';
            $sql .= 'VALUES(:name, :type, :host, :port, :username, :password, :spid, :spcode, :encoded, :extended, :service, :concurrent, :description)';
            $sth = $this->db->prepare($sql);

            foreach ($data as $key => $val) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
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

        if (isset($data['type'])) {
            $data['type'] = 1;
        }
        
        if (isset($data['encoded'])) {
            if (!in_array($data['encoded'], [0, 8, 15], true)) {
                unset($data['encoded']);
            }
        }

        if (!isset($data['extended'])) {
            $data['extended'] = 0;
        }
        
        if (isset($data['service'])) {
            $data['service'] = '{' . $data['service'] . '}';
        } else {
            $data['service'] = '{0}';
        }

        $column = $this->keyAssembly($data);

        if (count($data) > 0) {
            $sql = 'UPDATE ' . $this->table . ' SET ' . $column . ' WHERE id = :id';
            $sth = $this->db->prepare($sql);
            $sth->bindParam(':id', $id, PDO::PARAM_INT);

            foreach ($data as $key => $val) {
                if ($val !== null) {
                    $sth->bindParam(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
                }
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

    public function test($id = null, $phone = null, $message = null) {
        return true;
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
            case 'name':
                $res['name'] = Filter::string($val, null, 3, 32);
                break;
            case 'type':
                $res['type'] = Filter::number($val, null, 1);
                break;
            case 'host':
                $res['host'] = Filter::ip($val, null);
                break;
            case 'port':
                $res['port'] = Filter::port($val, null);
                break;
            case 'username':
                $res['username'] = Filter::alpha($val, null, 1, 32);
                break;
            case 'password':
                $res['password'] = Filter::string($val, null, 1, 64);
                break;
            case 'spid':
                $res['spid'] = Filter::alpha($val, null, 1, 32);
                break;
            case 'spcode':
                $res['spcode'] = Filter::alpha($val, null, 1, 32);
                break;
            case 'encoded':
                $res['encoded'] = Filter::number($val, null);
                break;
            case 'extended':
                $res['extended'] = Filter::number($val, 0, 0, 1);
                break;
            case 'service':
                $res['service'] = '0';
                if ($this->checkService($val)) {
                    $res['service'] = implode(',', $val);
                }
                break;
            case 'concurrent':
                $res['concurrent'] = Filter::number($val, null, 1);
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

    public function checkService(array $data) {
        $len = count($data);
        $service = [1, 2, 3, 4];
        
        if (!($len > 0 && $len <= count($service))) {
            return false;
        }

        foreach ($data as $val) {
            if (!in_array($val, $service, true)) {
                return false;
            }
        }

        return true;
    }
}
