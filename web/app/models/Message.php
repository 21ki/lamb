
<?php

/*
 * The Message Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class MessageModel {
    public $db = null;
    private $table = 'message';
    private $column = ['begin', 'end', 'spid', 'spcode', 'phone', 'content', 'status'];
    
    public function __construct() {
        try {
            $this->config = Yaf\Registry::get('config');
            $config = $this->config->msg;
            $options = [PDO::ATTR_PERSISTENT => true];
            $url = "pgsql:host={$config->host};port={$config->port};dbname={$config->name}";
            $this->db = new PDO($url, $config->user, $config->pass, $options);
            $this->db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
        } catch (PDOException $e) {
            error_log($e->getMessage(), 0);
        }
    }

    public function submitQuery(array $data) {
        $reply = [];
        $data = $this->checkArgs($data);

        $where = $this->whereAssembly($data);

        $sql = "SELECT * FROM {$this->table} WHERE {$where}";
        $sth = $this->db->prepare($sql);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }
        }

        if ($sth->execute()) {
            $reply = $sth->fetchAll();
        }

        return $reply;

    }

    public function checkArgs(array $data) {
        $reply = ['begin' => null,'end' => null, 'account' => null, 'spcode' => null, 'phone' => null, 'status' => null];

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'begin':
                $reply['begin'] = Filter::dateTime($val, null);
                break;
            case 'end':
                $reply['end'] = Filter::dateTime($val, null);
                break;
            case 'account':
                $reply['account'] = Filter::string($val, null, 1);
                break;
            case 'spcode':
                $reply['spcode'] = Filter::string($val, null, 1);
                break;
            case 'phone':
                $reply['phone'] = Filter::string($val, null, 1);
                break;
            case 'status':
                $reply['status'] = Filter::number($val, null, 1, 7);
                break;
            default:
                break;
            }
        }

        return $reply;
    }

    public function whereAssembly(array $data) {
        $where = '';

        if ($data['begin'] !== null && $data['end'] !== null) {
            $where .= 'create_time BETWEEN :begin AND :end';
        } else {
            return $where;
        }

        if ($data['account'] !== null) {
            $where .= ' AND account = :account';
        }

        if ($data['spcode'] !== null) {
            $where .= ' AND spcode = :spcode';
        }

        if ($data['phone'] !== null) {
            $where .= ' AND phone = :phone';
        }

        if ($data['status'] !== null) {
            $where .= ' AND status = :status';
        }

        return $where;
    }
}
