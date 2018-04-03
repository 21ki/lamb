<?php

/*
 * The Template Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class TemplateModel {
    public $db = null;
    private $table = 'template';
    private $column = ['acc', 'name', 'content'];
    
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

    public function getAll(int $acc = 0) {
        $result = [];
        if ($acc > 0) {
            $sql = 'SELECT * FROM ' . $this->table . ' WHERE acc = :acc ORDER BY id';
            $sth = $this->db->prepare($sql);
            $sth->bindValue(':acc', $acc, PDO::PARAM_INT);

            if ($sth->execute()) {
                $result = $sth->fetchAll();
            }
        }

        return $result;
    }

    public function create(array $data = null) {
        $data = $this->checkArgs($data);

        if (count($data) == count($this->column)) {
            $sql = 'INSERT INTO ' . $this->table . '(acc, name, content) VALUES(:acc, :name, :content)';
            $sth = $this->db->prepare($sql);

            foreach ($data as $key => $val) {
                $sth->bindParam(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
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
            case 'acc':
                $res['acc'] = Filter::number($val, null, 1);
                break;
            case 'name':
                $res['name'] = Filter::string($val, null, 1, 24);
                break;
            case 'content':
                $res['content'] = Filter::string($val, null, 1, 480);
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

    public function total(int $acc = 0) {
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
