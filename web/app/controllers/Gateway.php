<?php

/*
 * The Gateway Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class GatewayController extends Yaf\Controller_Abstract {
    public function init(){
        $this->request = $this->getRequest();
    }
    
    public function indexAction() {
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $gateway = new GatewayModel();
            $success = $gateway->create($this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;        
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $gateway = new GatewayModel();
            $success = $gateway->change($this->request->getQuery('id', null), $this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $gateway = new GatewayModel();
            $success = $gateway->delete($this->request->getQuery('id', null));
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function checkAction() {
        if ($this->request->isPost()) {
            $gateway = new GatewayModel();
            $id = $this->request->getQuery('id', null);
            $phone = $this->request->getPost('phone', null);
            $message = $this->request->getPost('message', null);
            $gateway->test($id, $phone, $message);
        }

        return false;
    }

    public function reportAction() {
        $request = $this->getRequest();

        $begin = $request->getQuery('begin', date('Y-m-d 00:00:00'));
        $end = $request->getQuery('end', date('Y-m-d 23:59:59'));

        $gateway = new GatewayModel();
        $gateways = [];
        foreach ($gateway->getAll() as $val) {
            $gateways[$val['id']] = $val['name'];
        }

        $this->getView()->assign('where', ['begin' => $begin, 'end' => $end]);
        $this->getView()->assign('gateways', $gateways);
        $this->getView()->assign('report', $gateway->report($begin, $end));

        return true;
    }
}


