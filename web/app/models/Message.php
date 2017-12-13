
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

    public function queryMessage(array $data, $limit = 50) {
        $reply = [];
        $data = $this->checkArgs($data);

        $where = $this->whereAssembly($data);

        $sql = "SELECT * FROM {$this->table} WHERE {$where} LIMIT :limit";
        $sth = $this->db->prepare($sql);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }
        }

        $sth->bindValue(':limit', $limit, PDO::PARAM_INT);

        if ($sth->execute()) {
            $reply = $sth->fetchAll();
        }

        return $reply;

    }

    public function queryDeliver(array $where, int $limit = 50) {
        $reply = [];
        $data = $this->checkDeliverArgs($where);
        $where = $this->whereDeliverAssembly($data);

        $sql = "SELECT * FROM delivery WHERE {$where} LIMIT :limit";
        $sth = $this->db->prepare($sql);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }
        }

        $sth->bindValue(':limit', $limit, PDO::PARAM_INT);

        if ($sth->execute()) {
            $reply = $sth->fetchAll();
        }

        return $reply;
    }

    public function queryReport(array $where, int $limit = 50) {
        $reply = [];
        $data = $this->checkReportArgs($where);
        $where = $this->whereReportAssembly($data);

        $sql = "SELECT * FROM report WHERE {$where} LIMIT :limit";
        $sth = $this->db->prepare($sql);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }
        }

        $sth->bindValue(':limit', $limit, PDO::PARAM_INT);

        if ($sth->execute()) {
            $reply = $sth->fetchAll();
        }

        return $reply;
    }

    public function queryStatistic(array $where, bool $grouping = false) {
        $data = $this->checkStatisticArgs($where);
        $where = $this->whereStatisicAssembly($data);

        $reply = ['type' => null, 'object' => null, 'status' => -1, 'total' => 0, 'begin' => null, 'end' => null];

        if ($data['company'] !== null) {
            $reply['type'] = '公司名称';
            $reply['object'] = $data['company'];
        } else if ($data['account'] !== null) {
            $reply['type'] = '帐号名称';
            $reply['object'] = $data['account'];
        } else if ($data['spcode'] !== null) {
            $reply['type'] = '接入号码';
            $reply['object'] = $data['spcode'];
        } else {
            return [];
        }

        if ($grouping) {
            $sql = "SELECT status, count(id) AS total FROM message WHERE {$where} GROUP BY status ORDER BY status ASC";
        } else {
            $sql = "SELECT count(id) AS total FROM message WHERE {$where}";
        }

        $sth = $this->db->prepare($sql);

        foreach ($data as $key => $val) {
            if ($val !== null) {
                $sth->bindValue(':' . $key, $data[$key], is_int($val) ? PDO::PARAM_INT : PDO::PARAM_STR);
            }
        }

        if ($sth->execute()) {
            $result = $sth->fetchAll();
            foreach ($result as &$r) {
                $r += $reply;
            }

            $reply = $result;
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

    public function checkDeliverArgs(array $data) {
        $reply = ['begin' => null,'end' => null, 'spcode' => null, 'phone' => null];

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'begin':
                $reply['begin'] = Filter::dateTime($val, null);
                break;
            case 'end':
                $reply['end'] = Filter::dateTime($val, null);
                break;
            case 'spcode':
                $reply['spcode'] = Filter::string($val, null, 1);
                break;
            case 'phone':
                $reply['phone'] = Filter::string($val, null, 1);
                break;
            default:
                break;
            }
        }

        return $reply;
    }

    public function whereDeliverAssembly(array $data) {
        $where = '';

        if ($data['begin'] !== null && $data['end'] !== null) {
            $where .= 'create_time BETWEEN :begin AND :end';
        } else {
            return $where;
        }

        if ($data['spcode'] !== null) {
            $where .= ' AND spcode = :spcode';
        }

        if ($data['phone'] !== null) {
            $where .= ' AND phone = :phone';
        }

        return $where;
    }

    public function checkReportArgs(array $data) {
        $reply = ['begin' => null,'end' => null, 'spcode' => null, 'phone' => null, 'status' => null];

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'begin':
                $reply['begin'] = Filter::dateTime($val, null);
                break;
            case 'end':
                $reply['end'] = Filter::dateTime($val, null);
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

    public function whereReportAssembly(array $data) {
        $where = '';

        if ($data['begin'] !== null && $data['end'] !== null) {
            $where .= 'create_time BETWEEN :begin AND :end';
        } else {
            return $where;
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

    public function checkStatisticArgs(array $data) {
        $reply = ['company' => null, 'account' => null, 'spcode' => null, 'status' => null, 'begin' => null,'end' => null];

        foreach ($data as $key => $val) {
            switch ($key) {
            case 'company':
                $reply['company'] = Filter::number($val, null, 1);
                break;
            case 'account':
                $reply['account'] = Filter::string($val, null, 1);
                break;
            case 'spcode':
                $reply['spcode'] = Filter::string($val, null, 1);
                break;
            case 'status':
                $reply['status'] = Filter::number($val, null, 1, 7);
                break;
            case 'begin':
                $reply['begin'] = Filter::dateTime($val, null);
                break;
            case 'end':
                $reply['end'] = Filter::dateTime($val, null);
                break;
            default:
                break;
            }
        }

        return $reply;
    }

    public function whereStatisicAssembly(array $data) {
        $where = '';

        if ($data['begin'] !== null && $data['end'] !== null) {
            $where .= 'create_time BETWEEN :begin AND :end';
        } else {
            return $where;
        }

        if ($data['status'] !== null) {
            $where .= ' AND status = :status';
        }

        if ($data['company'] !== null) {
            $where .= ' AND company = :company';
            return $where;
        }

        if ($data['account'] !== null) {
            $where .= ' AND account = :account';
            return $where;
        }

        if ($data['spcode'] !== null) {
            $where .= ' AND spcode = :spcode';
            return $where;
        }

        return $where;
    }
}
