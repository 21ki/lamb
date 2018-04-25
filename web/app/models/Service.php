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
        $core[] = $this->daemon();

        return $core;
    }

    public function ismg() {
        $lock = '/tmp/ismg.lock';
        $ismg['id'] = 1;
        $ismg['pid'] = $this->getPid($lock);
        $ismg['name'] = 'ismg';
        $ismg['status'] = $this->checkRuning($lock);
        $ismg['description'] = '客户端 CMPP 接入网关';

        return $ismg;
    }

    public function mt() {
        $lock = '/tmp/mt.lock';
        $mt['id'] = 1;
        $mt['pid'] = $this->getPid($lock);
        $mt['name'] = 'mt';
        $mt['status'] = $this->checkRuning($lock);
        $mt['description'] = '核心上行消息队列服务';

        return $mt;
    }

    public function mo() {
        $lock = '/tmp/mo.lock';
        $mo['id'] = 1;
        $mo['pid'] = $this->getPid($lock);
        $mo['name'] = 'mo';
        $mo['status'] = $this->checkRuning($lock);
        $mo['description'] = '核心下行消息队列服务';

        return $mo;
    }

    public function scheduler() {
        $lock = '/tmp/scheduler.lock';
        $scheduler['id'] = 1;
        $scheduler['pid'] = $this->getPid($lock);
        $scheduler['name'] = 'scheduler';
        $scheduler['status'] = $this->checkRuning($lock);
        $scheduler['description'] = '核心上行路由调度器服务';

        return $scheduler;
    }

    public function delivery() {
        $lock = '/tmp/delivery.lock';
        $delivery['id'] = 1;
        $delivery['pid'] = $this->getPid($lock);
        $delivery['name'] = 'delivery';
        $delivery['status'] = $this->checkRuning($lock);
        $delivery['description'] = '核心下行路由调度器服务';

        return $delivery;
    }    

    public function testd() {
        $lock = '/tmp/testd.lock';
        $testd['id'] = 1;
        $testd['pid'] = $this->getPid($lock);
        $testd['name'] = 'testd';
        $testd['status'] = $this->checkRuning($lock);
        $testd['description'] = '运营商网关通道测试服务';

        return $testd;
    }
    
    public function server() {
        $account = new AccountModel();
        $servers = [];

        foreach ($account->getAll() as $a) {
            $lock = '/tmp/serv-' . intval($a['id']) . '.lock';
            $servers[$a['id']]['id'] = $a['id'];
            $servers[$a['id']]['pid'] = $this->getPid($lock);
            $servers[$a['id']]['status'] = $this->checkRuning($lock);
            $servers[$a['id']]['description'] = 'no description';
        }

        return $servers;
    }

    public function gateway() {
        $gateway = new GatewayModel();
        $gateways = [];

        foreach ($gateway->getAll() as $g) {
            $lock = '/tmp/gtw-' . intval($g['id']) . '.lock';
            $gateways[$g['id']]['id'] = $g['id'];
            $gateways[$g['id']]['pid'] = $this->getPid($lock);
            $gateways[$g['id']]['status'] = $this->checkRuning($lock);
            $gateways[$g['id']]['description'] = 'no description';
        }

        return $gateways;
    }

    public function daemon() {
        $lock = '/tmp/daemon.lock';
        $daemon['id'] = 1;
        $daemon['pid'] = $this->getPid($lock);
        $daemon['name'] = 'daemon';
        $daemon['status'] = $this->checkRuning($lock);
        $daemon['description'] = '业务处理进程管理服务';

        return $daemon;
    }

    public function queue(string $type = 'mt') {
        $result = [];

        if (!in_array($type, ['mt', 'mo'], true)) {
            return $result;
        }

        $reply = $this->rdb->hGetAll($type . '.queue');

        if ($reply !== false) {
            foreach ($reply as $k => $v) {
                $result[$k]['id'] = $k;
                $result[$k]['total'] = $v;
                $result[$k]['description'] = 'no description';
            }
        }

        return $result;
    }

    public function checkRuning(string $file) {
        $pid = 0;
        $online = false;

        if (file_exists($file)) {
            $fp = fopen($file, 'r+');
            if ($fp) {
                if (flock($fp, LOCK_EX | LOCK_NB)) {
                    flock($fp, LOCK_UN);
                } else {
                    $pid = $this->getPid($file);
                    if (is_dir('/proc/' . $pid)) {
                        $online = true;    
                    }
                }
                fclose($fp);
            }
        }

        return $online;
    }

    public function getPid(string $file) {
        $pid = 0;

        if (file_exists($file) && is_file($file) && is_readable($file)) {
            $fp = fopen($file, "r");
            if ($fp) {
                $line = fgets($fp);
                if ($line !== false) {
                    $pid = intval($line);
                }
                fclose($fp);
            }
        }

        return $pid;
    }
}
