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
        return true;
    }

    public function checkChannelAction() {
        if ($this->request->isPost()) {
            $gateway = new GatewayModel();
            $gateway->checkMessage($this->request->getPost());
        }
        return false;
    }

    public function reportAction() {
        return true;
    }

    public function getreportAction() {
        if ($this->request->isGet()) {
            $gateway = new GatewayModel();
            $begin = urldecode($this->request->getQuery('begin'));
            $end = urldecode($this->request->getQuery('end'));
            $report = $gateway->report($begin, $end);

            $gateways = [];
            foreach ($gateway->getAll() as $g) {
                $gateways[$g['id']] = $g;
            }

            foreach ($report as &$r) {
                $r['gid'] = $gateways[$r['gid']]['name'];
            }

            lambResponse(200, 'success', $report);
        }

        return false;
    }
}


