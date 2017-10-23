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

    public function inbound(array $object) {
        $result = array();
        
        foreach ($object as $obj) {
            $pid = $this->getPid($obj['id'], 'client');
            if ($pid > 0 && is_dir('/proc/' . $pid)) {
                $result[$obj['id']]['username'] = $obj['username'];
                $result[$obj['id']]['company'] = $obj['company'];
                $result[$obj['id']]['route'] = $obj['route'];
                $result[$obj['id']]['queue'] = $this->getQueue($obj['id']);
                $result[$obj['id']]['deliver'] = $this->getDeliver($obj['id']);
                $result[$obj['id']]['speed'] = $this->getSpeed($obj['id'], 'client');
                $result[$obj['id']]['error'] = $this->getError($obj['id'], 'client');
            }
        }

        return $result;
    }

    public function outbound(array $object) {
        $result = array();

        foreach ($object as $obj) {
            $result[$obj['id']]['name'] = $obj['name'];
            $result[$obj['id']]['type'] = $obj['type'];
            $result[$obj['id']]['host'] = $obj['host'];
            $result[$obj['id']]['speed'] = $this->getSpeed($obj['id'], 'gateway');
            $result[$obj['id']]['error'] = $this->getError($obj['id'], 'gateway');
            if (is_dir('/proc/' . $this->getPid($obj['id'], 'gateway'))) {
                $result[$obj['id']]['status'] = $this->getStatus($obj['id'], 'gateway');
            } else {
                $result[$obj['id']]['status'] = -1;
            }
        }

        return $result;
    }
    
    public function getPid($id = null, $type = 'client') {
        $id = intval($id);
        $pid = 0;
        $reply = $this->redis->hGet($type . '.' . $id, 'pid');
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

    public function getSpeed($id = null, $type = 'client') {
        $id = intval($id);
        $speed = 0;
        $reply = $this->redis->hGet($type . '.' . $id, 'speed');
        if ($reply !== false) {
            $speed = intval($reply);
        }

        return $speed;
    }

    public function getError($id = null, $type = 'client') {
        $id = intval($id);
        $error = 0;
        $reply = $this->redis->hGet($type . '.' . $id, 'error');
        if ($reply !== false) {
            $error = intval($reply);
        }

        return $error;
    }

    public function getDeliver($id = null) {
        $id = intval($id);
        $deliver = 0;
        $reply = $this->redis->hGet('client.' . $id, 'deliver');
        if ($reply !== false) {
            $deliver = intval($reply);
        }

        return $deliver;
    }

    public function getStatus($id = null, $type = 'client') {
        $id = intval($id);

        $status = -1;

        $reply = $this->redis->hGet($type . '.' . $id, 'status');
        if ($reply !== false) {
            $status = intval($reply);
        }

        return $status;
    }
}
