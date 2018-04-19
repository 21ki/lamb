<?php

/*
 * The Service Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ServiceModel {
    public $db = null;
    public $rdb = null;
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
        $this->rdb = Yaf\Registry::get('rdb');
    }

    public function core() {
        $core['ismg'] = $this->checkRuning('/tmp/ismg.lock');
        $core['mt'] = $this->checkRuning('/tmp/mt.lock');
        $core['mo'] = $this->checkRuning('/tmp/mo.lock');
        $core['scheduler'] = $this->checkRuning('/tmp/scheduler.lock');
        $core['delivery'] = $this->checkRuning('/tmp/delivery.lock');
        $core['testd'] = $this->checkRuning('/tmp/testd.lock');

        return $core;
    }

    public function server() {
        return null;
    }

    public function checkRuning(string $file) {
        $fp = fopen($file, 'r+');

        if ($fp) {
            if (!flock($fp, LOCK_EX | LOCK_NB)) {
                fclose($fp);
                return true;
            }
            flock($fp, LOCK_UN);
            fclose($fp);
        }

        return false;
    }
}
