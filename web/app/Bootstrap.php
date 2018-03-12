<?php

/*
 * Yaf bootstrap
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

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
    
    public function _initDatabase() {
        $db = $this->config->db;
        $options = [PDO::ATTR_PERSISTENT => true];

        try {
            $url = 'pgsql:host=' . $db->host . ';port=' . $db->port . ';dbname=' . $db->name;
            $conn = new PDO($url, $db->user, $db->pass, $options);
            $conn->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);
            Yaf\Registry::set('db', $conn);
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
