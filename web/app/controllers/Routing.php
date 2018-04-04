<?php

/*
 * The Routing Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class RoutingController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }

    public function indexAction() {
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $success = false;
            $routing = new RoutingModel();
            $data = $this->request->getPost();
            $target = $this->getAccountId($data['rexp']);

            if ($target > 0) {
                $data['target'] = $target;
                $success = $routing->create($data);
            }

            lambResponse($success ? 200 : 400, $success ? 'success' : 'failed', null);
        }

        return false;
    }

    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $routing = new RoutingModel();
            $success = $routing->delete($this->request->getQuery('id', null));
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $success = false;
            $routing = new RoutingModel();
            $id = $this->request->getQuery('id', null);
            $data = $this->request->getPost();
            $success = $routing->change($id, $data);
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function upAction() {
        if ($this->request->isGet()) {
            $routing = new RoutingModel();
            $routing->move('up', $this->request->getQuery('id', null));
            lambResponse(200, 'success');
        }

        return false;
    }

    public function downAction() {
        if ($this->request->isGet()) {
            $routing = new RoutingModel();
            $routing->move('down', $this->request->getQuery('id', null));
            lambResponse(200, 'success');
        }

        return false;
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

    public function summaryAction() {
        if ($this->request->isGet()) {
            $account = new AccountModel();
            $accounts = $account->getAll();

            $channel = new ChannelModel();
            foreach ($accounts as &$a) {
                unset($a['password']);
                $a['total'] = $channel->total($a['id']);
            }

            lambResponse(200, 'success', $accounts);
        }

        return false;
    }
}
