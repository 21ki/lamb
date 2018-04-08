<?php

/*
 * Yaf bootstrap
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Db\Redis;

class Bootstrap extends Yaf\Bootstrap_Abstract{

    public function _initConfig() {
        $this->config = Yaf\Application::app()->getConfig();
        Yaf\Registry::set('config', $this->config);
    }

    public function _initAuth(Yaf\Dispatcher $dispatcher) {
        $authPlugin = new AuthPlugin();
        $dispatcher->registerPlugin($authPlugin);
    }

    public function _initCommon() {
        Yaf\Loader::import("common.php");
    }

    public function _initRedis() {
        $config = $this->config->redis;

        try {
            $redis = new Redis($config->host, $config->port, $config->password, $config->db);
            Yaf\Registry::set('rdb', $redis->handle);
        } catch (RedisException $e) {
            error_log($e->getMessage(), 0);
        }
    }

    public function _initDatabase() {
        $config = $this->config->db;
        $options = [PDO::ATTR_PERSISTENT => true];
        $url = 'pgsql:host=' . $config->host . ';port=' . $config->port . ';dbname=' . $config->name;

        try {
            $db = new PDO($url, $config->user, $config->pass, $options);
            $db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
            Yaf\Registry::set('db', $db);
        } catch (PDOException $e) {
            error_log($e->getMessage(), 0);
        }
    }

    public function _initSession(Yaf\Dispatcher $dispatcher) {
        $session = Yaf\Session::getInstance();
        Yaf\Registry::set('session', $session);
        $session->start();
    }
}
