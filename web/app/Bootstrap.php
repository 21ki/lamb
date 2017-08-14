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

    /*
    public function _initAuth(Yaf\Dispatcher $dispatcher) {
        $authPlugin = new AuthPlugin();
        $dispatcher->registerPlugin($authPlugin);
    }
    */

    public function _initCommon() {
        Yaf\Loader::import("common.php");
    }
    
    public function _initDatabase() {
        $host = $this->config->db->host;
        $port = $this->config->db->port;
        $user = $this->config->db->user;
        $pass = $this->config->db->pass;
        $name = $this->config->db->name;

        try {
            $db = new PDO('pgsql:host=' . $host . ';port=' . $port . ';dbname=' . $name, $user, $pass);
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
