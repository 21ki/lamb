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
    private $column = ['id', 'rid', 'weight', 'operator'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function get($id = null, $rid = null) {
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($id) . ' and rid = ' . intval($rid);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if ($result !== false) {
                return $result;
            }
        }

        return null;
    }

    public function getAll($rid = null) {
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE rid = ' . intval($rid) . ' ORDER BY weight';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetchAll();
        }

        return $result;
    }

    public function create(array $data = null) {
        $data = $this->checkArgs($data);

        if ($this->isExist($data['id']) && $this->check($data['id'], $data['rid'])) {
            $sql = 'INSERT INTO channels VALUES(:id, :rid, :weight, :operator)';
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

    public function delete($id = null, $rid = null) {
        $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . intval($id) . ' and rid = ' . intval($rid);
        if ($this->db->query($sql)) {
            return true;
        }
        return false;
    }

    public function deleteAll($rid = null) {
        $sql = 'DELETE FROM ' . $this->table . ' WHERE rid = ' . intval($rid);
        if ($this->db->query($sql)) {
            return true;
        }

        return false;
    }

    public function change(array $data = null) {
        $data = $this->checkArgs($data);
        $column = $this->keyAssembly($data);

        if (count($data) < 3) {
            return false;
        }
        
        if (!isset($data['id'], $data['rid'])) {
            return false;
        }

        $sql = 'UPDATE ' . $this->table . ' SET ' . $column . ' WHERE id = :id and rid = :rid';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $data['id'], PDO::PARAM_INT);
        $sth->bindValue(':rid', $data['rid'], PDO::PARAM_INT);

        foreach ($data as $key => $val) {
            if ($val != null) {
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

    public function check($id = null, $rid = null) {
        $id = intval($id);
        $rid = intval($rid);

        $sql = 'SELECT count(id) AS total FROM channels WHERE id = :id and rid = :rid';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);
        $sth->bindValue(':rid', $rid, PDO::PARAM_INT);

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
            case 'rid':
                $res['rid'] = Filter::number($val, null);
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
}
