<?php

/*
 * The Channel Group Model
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

    public function get($gid = null) {
        $group = [];
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($gid);
        $sth = $this->db->query($sql);
        if ($sth) {
            $group = $sth->fetch();
        }

        return $group;
    }
    
    public function getAll() {
        $groups = [];
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id';
        $sth = $this->db->query($sql);
        if ($sth) {
            $groups = $sth->fetchAll();
        }

        return $groups;
    }

    public function create() {
        return null;
    }

    public function delete($gid = null) {
        $gid = intval($gid);
        
        if (!$this->isUsed($gid)) {
            $sql = 'DELETE FROM groups WHERE id = ' . intval($gid);
            if ($this->db->query($sql)) {
                return true;
            }
        }
        return false;
    }

    public function isUsed($gid = NULL) {
        $sql = 'SELECT count(id) FROM account WHERE route = ' . intval($gid);
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
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
