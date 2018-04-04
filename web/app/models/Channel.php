<?php

/*
 * The Channel Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class ChannelModel {
    public $db = null;
    private $table = 'channels';
    private $column = ['id', 'acc', 'weight', 'operator'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function get($id = null, $acc = null) {
        $id = intval($id);
        $acc = intval($acc);
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = :id and acc = :acc';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);
        $sth->bindValue(':acc', $acc, PDO::PARAM_INT);

        if ($sth->execute()) {
            $result = $sth->fetch();
            if ($result !== false) {
                return $result;
            }
        }

        return null;
    }

    public function getAll($acc = null) {
        $result = [];
        $acc = intval($acc);
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE acc = :acc ORDER BY weight';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':acc', $acc, PDO::PARAM_INT);

        if ($sth->execute()) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function create(array $data = null) {
        $data = $this->checkArgs($data);

        if (!isset($data['operator'])) {
            $data['operator'] = 7;
        }
        
        if ($this->isExist($data['id']) && $this->check($data['id'], $data['acc'])) {
            $sql = 'INSERT INTO channels VALUES(:id, :acc, :weight, :operator)';
            $sth = $this->db->prepare($sql);

            foreach ($data as $key => $val) {
                if ($val !== null) {
                    $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
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

    public function delete($id = null, $acc = null) {
        $id = intval($id);
        $acc = intval($acc);
        $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . $id . ' and acc = ' . $acc;
        if ($this->db->query($sql)) {
            return true;
        }

        return false;
    }

    public function deleteAll($acc = null) {
        $acc = intval($acc);
        $sql = 'DELETE FROM ' . $this->table . ' WHERE acc = ' . $acc;
        if ($this->db->query($sql)) {
            return true;
        }

        return false;
    }

    public function change($id = null, $acc = null, array $data = null) {
        $id = intval($id);
        $acc = intval($acc);
        $data = $this->checkArgs($data);
        $column = $this->keyAssembly($data);

        if (count($data) < 1) {
            return false;
        }
        
        $sql = 'UPDATE ' . $this->table . ' SET ' . $column . ' WHERE id = :id and acc = :acc';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);
        $sth->bindValue(':acc', $acc, PDO::PARAM_INT);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }
        }

        if ($sth->execute()) {
            return true;

        }

        return false;
    }
    
    public function isExist($id = null) {
        $sql = 'SELECT count(id) AS total FROM gateway WHERE id = ' . intval($id);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if (intval($result['total']) > 0) {
                return true;
            }
        }

        return false;
    }

    public function check($id = null, $acc = null) {
        $id = intval($id);
        $acc = intval($acc);

        $sql = 'SELECT count(id) AS total FROM channels WHERE id = :id and acc = :acc';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);
        $sth->bindValue(':acc', $acc, PDO::PARAM_INT);

        if ($sth->execute()) {
            $result = $sth->fetch();
            if (intval($result['total']) == 0) {
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
            case 'id':
                $res['id'] = Filter::number($val, null);
                break;
            case 'acc':
                $res['acc'] = Filter::number($val, null);
                break;
            case 'weight':
                $res['weight'] = Filter::number($val, 1, 1);
                break;
            case 'operator':
                $res['operator'] = Filter::number($val, 0);
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

    public function total(int $acc) {
        $total = 0;

        if ($acc > 0) {
            $sql = 'SELECT count(id) FROM ' . $this->table . ' WHERE acc = :acc';
            $sth = $this->db->prepare($sql);
            $sth->bindValue(':acc', $acc, PDO::PARAM_INT);

            if ($sth->execute()) {
                $result = $sth->fetch();
                if (isset($result['count'])) {
                    $total = intval($result['count']);
                }
            }
        }

        return $total;
    }
}
