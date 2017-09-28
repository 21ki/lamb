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
    
    public function groupAction() {
        $id = $this->request->getQuery('id', null);
        $group = new GroupModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $group->get($id);
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

    public function channelAction() {
        $channel = new ChannelModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $channel->get($this->request->getQuery('id', null), $this->request->getQuery('gid', null));

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
}
