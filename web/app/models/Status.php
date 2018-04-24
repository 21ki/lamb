<?php

/*
 * The Status Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class StatusModel {
    public $db = null;
    public $rdb = null;
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->rdb = Yaf\Registry::get('rdb');
    }

    public function inbound() {
        $result = array();
        $account = new AccountModel();

        foreach ($account->getAll() as $a) {
            if ($this->isOnline($a['id'], 5)) {
                $result[$a['id']]['username'] = $a['username'];
                $result[$a['id']]['company'] = $a['company'];
                $result[$a['id']]['address'] = $this->getAddress($a['id']);
                $result[$a['id']]['status'] = 1;
                $result[$a['id']]['speed'] = $this->getSpeed($a['id'], 'client');
                $result[$a['id']]['error'] = $this->getError($a['id'], 'client');
            }
        }

        return $result;
    }

    public function outbound() {
        $result = array();
        $gateway = new GatewayModel();
        
        foreach ($gateway->getAll() as $g) {
            $result[$g['id']]['name'] = $g['name'];
            $result[$g['id']]['type'] = $g['type'];
            $result[$g['id']]['host'] = $g['host'];
            $result[$g['id']]['speed'] = $this->getSpeed($g['id'], 'gateway');
            $result[$g['id']]['error'] = $this->getError($g['id'], 'gateway');
            if ($this->checkRuning($g['id'])) {
                $result[$g['id']]['status'] = $this->getStatus($g['id'], 'gateway');
            } else {
                $result[$g['id']]['status'] = -1;
            }
        }

        return $result;
    }

    public function getQueue($id = null) {
        $id = intval($id);
        $queue = 0;
        $reply = $this->rdb->hGet('client.' . $id, 'queue');
        if ($reply !== false) {
            $queue = intval($reply);
        }

        return $queue;
    }

    public function getSpeed($id = null, $type = 'client') {
        $id = intval($id);
        $speed = 0;
        $reply = $this->rdb->hGet($type . '.' . $id, 'speed');
        if ($reply !== false) {
            $speed = intval($reply);
        }

        return $speed;
    }

    public function getError($id = null, $type = 'client') {
        $id = intval($id);
        $error = 0;
        $reply = $this->rdb->hGet($type . '.' . $id, 'error');
        if ($reply !== false) {
            $error = intval($reply);
        }

        return $error;
    }

    public function getDeliver($id = null) {
        $id = intval($id);
        $deliver = 0;
        $reply = $this->rdb->hGet('client.' . $id, 'deliver');
        if ($reply !== false) {
            $deliver = intval($reply);
        }

        return $deliver;
    }

    public function getStatus($id = null, $type = 'client') {
        $id = intval($id);

        $status = -1;

        $reply = $this->rdb->hGet($type . '.' . $id, 'status');
        if ($reply !== false) {
            $status = intval($reply);
        }

        return $status;
    }

    public function getAddress($id = null) {
        $id = intval($id);
        $addr = 'unknown';

        $reply = $this->rdb->hGet('client.' . $id, 'addr');
        if ($reply !== false) {
            $addr = $reply;
        }

        return $addr;
    }
    
    public function isOnline($id = 0, $interval = 0) {
        $id = intval($id);

        $current = time();
        $reply = $this->rdb->hGet('client.' . $id, 'online');

        if (($current - intval($reply)) < $interval) {
            return true;
        }

        return false;
    }

    public function checkRuning(int $id = 0) {
        $online = false;
        $file = '/tmp/gtw-' . $id . '.lock';
        $pid = 0;

        if (file_exists($file)) {
            $fp = fopen($file, 'r+');
            if ($fp) {
                if (flock($fp, LOCK_EX | LOCK_NB)) {
                    flock($fp, LOCK_UN);
                } else {
                    $online = true;
                }
                fclose($fp);
            }
        }

        return $online;
    }
}
