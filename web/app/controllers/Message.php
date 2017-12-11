<?php

/*
 * The Message Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class MessageController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }
    
    public function indexAction() {
        return true;
    }

    public function deliverAction() {
        return true;
    }

    public function reportAction() {
        return true;
    }

    public function statisticAction(){
        return true;
    }
}


