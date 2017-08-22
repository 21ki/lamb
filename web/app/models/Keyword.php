<?php

/*
 * The Keyword Model
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class KeywordModel {
    public $db = null;
    
    public function __construct() {
        $this->db = Yaf\Registry::get('db');
    }
}
