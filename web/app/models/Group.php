<?php

/*
 * The Group Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class GroupModel {
    public $db = null;
    private $table = 'groups';
    private $column = ['name', 'description'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function get(int $id = null) {
        $result = [];
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($id);
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

        $sql = 'INSERT INTO ' . $this->table . '(name, description) VALUES(:name, :description) returning id';
        $sth = $this->db->prepare($sql);

        if (isset($data['name'], $data['description'])) {
            $sth->bindParam(':name', $data['name'], PDO::PARAM_STR);
            $sth->bindValue(':description', $data['description'], PDO::PARAM_STR);
            if ($sth->execute()) {
                $result = $sth->fetch();
                return intval($result['id']);
            }
        }

        return 0;
    }

    public function delete($id = null) {
        $id = intval($id);

        if (!$this->isUsed($id)) {
            $sql = 'DELETE FROM ' . $this->table . ' WHERE id = ' . intval($id);
            if ($this->db->query($sql)) {
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
    
    public function isUsed($id = NULL) {
        $sql = 'SELECT count(id) AS total FROM account WHERE route = ' . intval($id);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if (intval($result['total']) > 0) {
                return true;
            }
        }

        return false;
    }

    public function total($id = null) {
        $count = 0;
        $sql = 'SELECT count(id) AS total FROM channels WHERE rid = ' . intval($id);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            $count = intval($result['total']);
        }

        return $count;
    }

    public function isExist($id = null) {
        $sql = 'SELECT count(id) AS total FROM ' . $this->table . ' WHERE id = ' . intval($id);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if (intval($result['total']) > 0) {
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
