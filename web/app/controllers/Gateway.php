<?php

/*
 * The Gateway Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class GatewayController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $gateway = new GatewayModel();
        $this->getView()->assign('gateways', $gateway->getAll());
        return true;
    }

    public function createAction() {
        $request =  $this->getRequest();

        if ($request->isPost()) {
            $gateway = new GatewayModel();
            $form = $request->getPost();

            if (isset($form['service']) && is_array($form['service'])) {
                array_walk($form['service'], 'str2int');
                $form['service'] = array_unique($form['service']);
            } else {
                $form['service'] = [];
            }

            $gateway->create($form);
            $response = $this->getResponse();
            $response->setRedirect('/gateway');
            $response->response();
            return false;
            
        }
        
        return true;
    }

    public function editAction() {
        $request = $this->getRequest();
        $gateway = new GatewayModel();

        if ($request->isPost()) {
            $form = $request->getPost();

            if (isset($form['service']) && is_array($form['service'])) {
                array_walk($form['service'], 'str2int');
                $form['service'] = array_unique($form['service']);
            } else {
                $form['service'] = [];
            }
            
            $gateway->change($request->getPost('id'), $form);
            $response = $this->getResponse();
            $response->setRedirect('/gateway');
            $response->response();
            return false;
        }

        $data = $gateway->get($request->getQuery('id'));
        $data['service'] = explode(',', trim($data['service'], '{}'));
        $this->getView()->assign('gateway', $data);
        return true;
    }

    public function deleteAction() {
        $success = false;
        $request = $this->getRequest();

        $id = $request->getQuery('id', null);
        $gateway = new GatewayModel();
        if ($gateway->delete($id)) {
            $success = true;
        }

        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function testAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $channel = new GatewayModel();
            $channel->test($request->getPost('id'), $request->getPost('phone'), $request->getPost('message'));
        }

        $response = $this->getResponse();
        $response->setRedirect('/gateway');
        $response->response();

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


