<?php

/*
 * The Database Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class DatabaseModel {
    public $db = null;
    private $table = 'database';
    private $column = ['name', 'type', 'description'];

    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function get($id = null) {
        $id = intval($id);
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
        }

        return $result;
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

        $id = $this->getLastId() + 1;
        $sql = 'INSERT INTO ' . $this->table . '(id, name, type, total, description) VALUES(:id, :name, :type, 0, :description)';
        $sth = $this->db->prepare($sql);

        $sth->bindValue(':id', $id, PDO::PARAM_INT);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            } else {
                return false;
            }
        }

        if (!$sth->execute()) {
            return false;
        }

        return true;
    }
    
    public function delete($id = null) {
        $id = intval($id);
        $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . $id;
        if ($this->db->query($sql)) {
            return true;
        }

        return false;
    }

    public function checkArgs(array $data) {
        $res = [];
        $data = array_intersect_key($data, array_flip($this->column));

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'name':
                $res['name'] = Filter::string($val, null, 1);
                break;
            case 'type':
                $res['type'] = Filter::number($val, 1, 1, 2);
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

    public function getLastId() {
        $id = 0;
        $sql = 'SELECT max(id) as id FROM ' . $this->table;
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            $id = intval($result['id']);
        }

        return $id;
    }
}
