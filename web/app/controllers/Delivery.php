<?php

/*
 * The Delivery Route Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class DeliveryController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }
    
    public function indexAction() {
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $delivery = new DeliveryModel();
            $success = $delivery->create($this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }
        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $delivery = new DeliveryModel();
            $success = $delivery->change($this->request->getQuery('id', null), $this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $delivery = new DeliveryModel();
            $success = $delivery->delete($this->request->getQuery('id', null));
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }
}
