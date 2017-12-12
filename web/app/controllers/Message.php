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

    public function getmessageAction() {
        if ($this->request->isGet()) {
            $message = new MessageModel();
            $where = $this->request->getQuery();
            $where['begin'] = urldecode($this->request->getQuery('begin', null));
            $where['end'] = urldecode($this->request->getQuery('end', null));

            if (isset($where['type']) && isset($where['object'])) {
                if ($where['type'] == 1) {
                    $where['spcode'] = $where['object'];
                } else {
                    $where['phone'] = $where['object'];
                }
                unset($where['type'], $where['object']);
            }

            lambResponse(200, 'success', $message->submitQuery($where));
        }

        return false;
    }
}


