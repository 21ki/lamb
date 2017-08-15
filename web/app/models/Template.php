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
    private $column = ['name', 'contents', 'account', 'description'];
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function get($id = null) {
        $sql = 'SELECT * FROM ' . $this->table . ' WHERE id = ' . intval($id);
        $result = $this->db->query($sql)->fetch();
        return $result;
    }

    public function getAll() {
        $sql = 'SELECT * FROM ' . $this->table . ' ORDER BY id';
        $result = $this->db->query($sql)->fetchAll();
        return $result;
    }

    public function create(array $data = null) {
        $data = $this->checkArgs($data);
        
        if (isset($data['account'])) {
            $account = new AccountModel();
            if (!$account->isExist($data['account'])) {
                return false;
            }
        }
        
        if (count($data) == count($this->column)) {
            $sql = 'INSERT INTO ' . $this->table . '(name, contents, account, description) ';
            $sql .= 'VALUES(:name, :contents, :account, :description)';
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
    
    public function checkArgs(array $data) {
        $res = array();
        $data = array_intersect_key($data, array_flip($this->column));

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'name':
                $res['name'] = Filter::string($val, null, 3, 24);
                break;
            case 'contents':
                $res['contents'] = Filter::string($val, null, 3, 480);
                break;
            case 'account':
                $res['account'] = Filter::number($val, null);
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
