<?php

/*
 * The Message Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class MessageController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $this->getView()->assign('messages', []);
        return true;
    }

    public function deliverAction() {
        $this->getView()->assign('delivers', []);
        return true;
    }

    public function reportAction() {
        $this->getView()->assign('reports', []);
        return true;
    }
}


