<?php

/*
 * The Api Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ApiController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        if (!$this->request->isXmlHttpRequest()) {
            exit;
        }
    }

    public function accountsAction() {
        $account = new AccountModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $account->getAll();

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
    
    public function routingAction() {
        $id = $this->request->getQuery('id', null);
        $routing = new RoutingModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $routing->get($id);
        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function routingsAction() {
        $routing = new RoutingModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $routing->getAll();

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function channelAction() {
        $channel = new ChannelModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $channel->get($this->request->getQuery('id', null), $this->request->getQuery('rid', null));

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function gatewaysAction() {
        $gateway = new GatewayModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $gateway->getAll();

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function templateAction() {
        $template = new TemplateModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $template->get($this->request->getQuery('id'));

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
}
