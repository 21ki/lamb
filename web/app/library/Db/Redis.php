<?php

/*
 * The Redis driver
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

namespace Db;

class Redis {
	public $handle = null;
	public $host = '127.0.0.1';
	public $port = 6379;
	public $password = null;
	public $database = 0;

	public function __construct($host = '127.0.0.1', $port = 6379, $password = null, $database = 0) {
        	/* Check redis extension */
    	    if (!extension_loaded('redis')) {
    	    	error_log('Unable to find redis driver extension', 0);
    	    	return $this->handle;
    	    }

    	    $this->host = $host;
    	    $this->port = $port;
    	    $this->password = $password;
    	    $this->database = $database;

    	    $this->handle = new \Redis();
    	    $this->handle->connect($host, $port);

    	    if (!empty($password)) {
    	    	$this->handle->auth($password);
    	    }

    	    $this->handle->select($database);
    }
}

