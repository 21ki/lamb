<?php

/*
 * The Status Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Db\Redis;

class StatusModel {
    public $db = null;
    public $redis = null;
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->config = Yaf\Registry::get('config');
        if ($this->config) {
            $config = $this->config->backup;
            $redis = new Redis($config->host, $config->port, $config->password, $config->db);
            $this->redis = $redis->handle;
        }
    }

    public function online(array $accounts) {
        $result = array();
        
        foreach ($accounts as $a) {
            $pid = $this->getPid($a['id']);
            if ($pid > 0 && is_dir('/proc/' . $pid)) {
                $result[$a['id']]['username'] = $a['username'];
                $result[$a['id']]['company'] = $a['company'];
                $result[$a['id']]['route'] = $a['route'];
                $result[$a['id']]['queue'] = $this->getStatus($a['id']);
                $result[$a['id']]['speed'] = 1000;
            }
        }

        return $result;
    }

    public function getPid($id = null) {
        $pid = 0;
        $result = $this->redis->get('client.' . intval($id));
        if ($result !== false) {
            $pid = intval($result);
        }

        return $pid;
    }

    public function getStatus($id = null) {
        return $this->redis->hGet('server.' . intval($id), 'queue');
    }
}
