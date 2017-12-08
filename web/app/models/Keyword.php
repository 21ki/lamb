<?php

/*
 * The Keyword Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class KeywordModel {
    public $db = null;
    private $table = 'keyword';
    private $column = ['val', 'tag'];

    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }

    public function getTags() {
        $reply = [];
        $sql = 'SELECT tag FROM ' . $this->table . ' GROUP BY tag';
        $sth = $this->db->query($sql);
        if ($sth) {
            $reply = $sth->fetchAll();
        }

        return $reply;
    }

    public function getClassification(string $tag = null) {
        $reply = [];
        $sql = 'SELECT * FROM keyword WHERE tag = :tag';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':tag', $tag, PDO::PARAM_STR);
        if ($sth->execute()) {
            $reply = $sth->fetchAll();
        }

        return $reply;
    }

    public function getAllClassification() {
        $index = 0;
        $reply = [];

        foreach ($this->getTags() as $tag) {
            $reply[$index]['tag'] = $tag['tag'];
            $reply[$index]['vals'] = $this->getClassification($tag['tag']);
            $index++;
        }

        return $reply;
    }

    public function create(array $data) {
        $data = $this->checkArgs($data);

        if (count($data) > 0) {
            $sql = 'INSERT INTO ' . $this->table . '(val, tag) VALUES(:val, :tag)';
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

    public function delete(int $id): bool {
        $sql = 'DELETE FROM ' . $this->table . ' WHERE id = :id';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':id', $id, PDO::PARAM_INT);
        if ($sth->execute()) {
            return true;
        }

        return false;
    }
    
    public function deleteClassification(string $tag): bool {
        if (strlen($tag) < 1) {
            return false;
        }

        $sql = 'DELETE FROM ' . $this->table . ' WHERE tag = :tag';
        $sth = $this->db->prepare($sql);
        $sth->bindValue(':tag', $tag, PDO::PARAM_STR);
        if ($sth->execute()) {
            return true;
        }

        return false;
    }

    public function checkArgs(array $data) {
        $reply = [];
        $data = array_intersect_key($data, array_flip($this->column));

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'val':
                $reply['val'] = Filter::string($val, null, 1);
                break;
            case 'tag':
                $reply['tag'] = Filter::string($val, 'default', 1);
                break;
            default:
                break;
            }
        }

        return $reply;
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
