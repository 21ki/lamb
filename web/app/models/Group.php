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

    public function get($id = null) {
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($id);
        $group = $this->db->query($sql)->fetch();
        if (count($group) > 0) {
            $group['channels'] = $this->getChannels($id);
            return $group;
        }

        return $group;
    }
    
    public function getAll() {
        $groups = array();
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id';
        $sth = $this->db->query($sql);
        if ($sth !== false) {
            $groups =  $sth->fetchAll();
            foreach ($groups as &$group) {
                $group['channels'] = $this->getChannels($group['id']);
            }
        }

        return $groups;
    }

    public function create(array $data, array $channel = null, array $weight = null) {
        $data = $this->checkArgs($data);
        $channels = array();

        foreach ($channel as $key => $val) {
            if (isset($weight[$key])) {
                $channels[$key]['channel'] = $val;
                $channels[$key]['weight'] = $weight[$key];
            }
        }

        return $channels;
    }

    public function delete($id = null) {
        $sql = 'DELETE FROM channels WHERE gid = ' . intval($id);
        $this->db->query($sql);

        $sql = 'DELETE FROM groups WHERE id = ' . intval($id);
        if ($this->db->query($sql)) {
            return true;
        }

        return false;
    }

    public function getChannels($gid = null) {
        $sql = 'SELECT * FROM channels WHERE gid = ' . intval($gid);
        $result = $this->db->query($sql)->fetchAll();
        return $result;
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
