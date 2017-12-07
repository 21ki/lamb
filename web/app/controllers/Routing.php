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
            $routing = new RoutingModel();
            $success = $routing->create($this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
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
            $routing = new RoutingModel();
            $success = $routing->change($this->request->getQuery('id', null), $this->request->getPost());
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

}
