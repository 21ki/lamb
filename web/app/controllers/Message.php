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

            lambResponse(200, 'success', $message->queryReport($where));
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
                    $where['company'] = $this->getCompanyId($where['object']);
                } else if ($where['type'] == 2) {
                    $where['account'] = $this->getAccountId($where['object']);
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

    public function getCompanyId(string $name) {
        $id = 0;
        $db = Yaf\Registry::get('db');
        $name = str_replace([" ", "\n", "\r", "\t"], '', $name);
        $sql = 'SELECT id FROM company WHERE name = :name';
        $sth = $db->prepare($sql);
        $sth->bindValue(':name', $name, PDO::PARAM_STR);

        if ($sth->execute()) {
            $reply = $sth->fetch();
            if (count($reply) > 0) {
                $id = intval($reply['id']);
            }
        }

        return $id;
    }

    public function getAccountId(string $username) {
        $id = 0;
        $db = Yaf\Registry::get('db');
        $username = str_replace([" ", "\n", "\r", "\t"], '', $username);
        $sql = 'SELECT id FROM account WHERE username = :username';
        $sth = $db->prepare($sql);
        $sth->bindValue(':username', $username, PDO::PARAM_STR);

        if ($sth->execute()) {
            $reply = $sth->fetch();
            if (count($reply) > 0) {
                $id = intval($reply['id']);
            }
            
        }

        return $id;
    }
}


