<?php

/*
 * The Routing Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class RoutingController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
    }

    public function indexAction() {
        $request = $this->getRequest();
        $routing = new RoutingModel();
        $routings = $routing->getAll();

        foreach ($routings as &$r) {
            $r['total'] = $routing->total($r['id']);
        }

        $gateway = new GatewayModel();
        
        $this->getView()->assign('groups', $routings);
        $this->getView()->assign('gateways', $gateway->getAll());

        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $routing = new RoutingModel();
            $routing->create($this->request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/routing');
            $response->response();
        }
        return false;
    }
    public function deleteAction() {
        $success = false;
        $id = $this->request->getQuery('id');
        $routing = new RoutingModel();
        if ($routing->delete($id)) {
            $channel = new ChannelModel();
            $success = $channel->deleteAll($id);
        }
        
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
    public function updateAction() {
        if ($this->request->isPost()) {
            $routing = new RoutingModel();
            $id = $this->request->getPost('id', null);
            $routing->change($id, $this->request->getPost());
        }
        $this->response->setRedirect('/routing');
        $this->response->response();
        return false;
    }
}
