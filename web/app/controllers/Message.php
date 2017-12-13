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

            $limit = $this->request->getQuery('limit', 50);
            lambResponse(200, 'success', $message->queryMessage($where, $limit));
        }

        return false;
    }

    public function getdeliverAction() {
        if ($this->request->isGet()) {
            $message = new MessageModel();
            $where = $this->request->getQuery();
            $where['begin'] = urldecode($this->request->getQuery('begin', null));
            $where['end'] = urldecode($this->request->getQuery('end', null));
            $limit = $this->request->getQuery('limit', 50);

            lambResponse(200, 'success', $message->queryDeliver($where, $limit));
        }

        return false;
    }

    public function getreportAction() {
        if ($this->request->isGet()) {
            $message = new MessageModel();
            $where = $this->request->getQuery();
            $where['begin'] = urldecode($this->request->getQuery('begin', null));
            $where['end'] = urldecode($this->request->getQuery('end', null));
            $limit = $this->request->getQuery('limit', 50);

            lambResponse(200, 'success', $message->queryReport($where, $limit));
        }

        return false;
    }

    public function getstatisticAction() {
        if ($this->request->isGet()) {
            $message = new MessageModel();
            $where = $this->request->getQuery();
            $where['begin'] = urldecode($this->request->getQuery('begin', null));
            $where['end'] = urldecode($this->request->getQuery('end', null));

            if (isset($where['type']) && isset($where['object'])) {
                if ($where['type'] == 1) {
                    $where['company'] = $where['object'];
                } else if ($where['type'] == 2) {
                    $where['account'] = $where['object'];
                } else if ($where['type'] == 3) {
                    $where['spcode'] = $where['object'];
                } else {
                    lambResponse(400, 'success');
                    return false;
                }

                unset($where['type'], $where['object']);
            } else {
                lambResponse(400, 'success');
                return false;
            }

            if ($this->request->getQuery('grouping', null) !== null) {
                lambResponse(200, 'success', $message->queryStatistic($where, true));
            } else {
                lambResponse(200, 'success', $message->queryStatistic($where, false));
            }
        }

        return false;
    }
}


