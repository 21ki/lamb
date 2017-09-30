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
                $result[$a['id']]['queue'] = $this->getQueue($a['id']);
                $result[$a['id']]['speed'] = $this->getSpeed($a['id']);
                $result[$a['id']]['error'] = $this->getError($a['id']);
            }
        }

        return $result;
    }

    public function getPid($id = null) {
        $id = intval($id);
        $pid = 0;
        $reply = $this->redis->hGet('client.' . $id, 'pid');
        if ($reply !== false) {
            $pid = intval($reply);
        }

        return $pid;
    }

    public function getQueue($id = null) {
        $id = intval($id);
        $queue = 0;
        $reply = $this->redis->hGet('client.' . $id, 'queue');
        if ($reply !== false) {
            $queue = intval($reply);
        }

        return $queue;
    }

    public function getSpeed($id = null) {
        $id = intval($id);
        $speed = 0;
        $reply = $this->redis->hGet('client.' . $id, 'speed');
        if ($reply !== false) {
            $speed = intval($reply);
        }

        return $speed;
    }

    public function getError($id = null) {
        $id = intval($id);
        $error = 0;
        $reply = $this->redis->hGet('client.' . $id, 'error');
        if ($reply !== false) {
            $error = intval($reply);
        }

        return $error;
    }
}
