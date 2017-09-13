<?php

/*
 * The Login Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Db\Redis;
use Tool\Filter;

class LoginModel {
    public $redis = null;
    public $config = null;
    private $username = null;
    private $password = null;
      
    public function __construct(array $data = null) {
        if (isset($data['username'], $data['password'])) {
            $this->username = Filter::alpha($data['username'], null, 1, 32);
            $this->password = Filter::string($data['password'], null, 6, 64);

            if ($this->username && $this->password) {
                $this->config = Yaf\Registry::get('config');

                if ($this->config) {
                    $config = $this->config->redis;
                    $redis = new Redis($config->host, $config->port, $config->password, $config->db);
                    $this->redis = $redis->handle;
                }

                $this->password = sha1(md5($this->password));
            }
        }
    }

    public function verify() {
        if ($this->redis && $this->username && $this->password) {
            $encryption = $this->redis->hGet('admin', 'password');
            if ($this->password === $encryption) {
                return true;
            }
        }

        return false;
    }
}
