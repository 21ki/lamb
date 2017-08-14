<?php

/*
 * The System Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class SystemController extends Yaf\Controller_Abstract {
    public function indexAction() {
        return true;
    }

    public function blacklistAction() {
        return true;
    }

    public function whitelistAction() {
        return true;
    }

    public function aboutAction() {
        return true;
    }
}


