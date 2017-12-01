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

        $this->request = $this->getRequest();
    }

    public function accountAction() {
        $account = new AccountModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $account->get($this->request->getQuery('id', null));

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
    
    public function accountsAction() {
        $account = new AccountModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $account->getAll();

        foreach ($response['data'] as &$val) {
            unset($val['password']);
        }
        
        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
    
    public function routingAction() {
        $routing = new RoutingModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $routing->get($this->request->getQuery('id', null));

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

        foreach ($response['data'] as &$g) {
            unset($g['password']);
        }
        
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

    public function templatesAction() {
        if ($this->request->isGet()) {
            $template = new TemplateModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $template->getAll();

            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }

    public function groupAction() {
        $group = new GroupModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $group->get($this->request->getQuery('id', null));

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function groupsAction() {
        $group = new GroupModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $group->getAll();

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
}
