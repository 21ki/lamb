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
        $core[] = $this->ismg();
        $core[] = $this->mt();
        $core[] = $this->mo();
        $core[] = $this->scheduler();
        $core[] = $this->delivery();
        $core[] = $this->testd();

        return $core;
    }

    public function ismg() {
        $ismg['id'] = 1;
        $ismg['pid'] = 100;
        $ismg['name'] = 'ismg';
        $ismg['status'] = $this->checkRuning('/tmp/ismg.lock');
        $ismg['description'] = '客户端 CMPP 接入网关';

        return $ismg;
    }

    public function mt() {
        $mt['id'] = 1;
        $mt['pid'] = 100;
        $mt['name'] = 'mt';
        $mt['status'] = $this->checkRuning('/tmp/mt.lock');
        $mt['description'] = '核心上行消息队列服务';

        return $mt;
    }

    public function mo() {
        $mo['id'] = 1;
        $mo['pid'] = 100;
        $mo['name'] = 'mo';
        $mo['status'] = $this->checkRuning('/tmp/mo.lock');
        $mo['description'] = '核心下行消息队列服务';

        return $mo;
    }

    public function scheduler() {
        $scheduler['id'] = 1;
        $scheduler['pid'] = 100;
        $scheduler['name'] = 'scheduler';
        $scheduler['status'] = $this->checkRuning('/tmp/scheduler.lock');
        $scheduler['description'] = '核心上行路由调度器服务';

        return $scheduler;
    }

    public function delivery() {
        $delivery['id'] = 1;
        $delivery['pid'] = 100;
        $delivery['name'] = 'delivery';
        $delivery['status'] = $this->checkRuning('/tmp/delivery.lock');
        $delivery['description'] = '核心下行路由调度器服务';

        return $delivery;
    }    

    public function testd() {
        $testd['id'] = 1;
        $testd['pid'] = 100;
        $testd['name'] = 'testd';
        $testd['status'] = $this->checkRuning('/tmp/testd.lock');
        $testd['description'] = '运营商网关通道测试服务';

        return $testd;
    }
    
    public function server() {
        return null;
    }

    public function checkRuning(string $file) {
        $online = false;
        $fp = fopen($file, 'r+');

        if ($fp) {
            if (flock($fp, LOCK_EX | LOCK_NB)) {
                flock($fp, LOCK_UN);
            } else {
                $online = true;
            }
            fclose($fp);
        }

        return $online;
    }
}
