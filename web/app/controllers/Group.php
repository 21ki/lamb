<?php

/*
 * The Group Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class GroupController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $gid = $this->getRequest()->getQuery('id', null);
        $gateway = new GatewayModel();
        $gateways = [];

        foreach ($gateway->getAll() as $g) {
            $gateways[$g['id']] = $g['id'];
            $gateways[$g['id']] = $g['name'];
        }

        $channel = new ChannelModel();
        $this->getView()->assign('gid', intval($gid));
        $this->getView()->assign('channels', $channel->getAll($gid));
        $this->getView()->assign('gateways', $gateways);
        return true;
    }
    
    public function createAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $group = new GroupModel();
            $group->create($request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/routing');
            $response->response();
        }

        return false;
    }

    public function deleteAction() {
        $success = false;
        $id = $this->getRequest()->getQuery('id');

        $group = new GroupModel();
        if ($group->delete($gid)) {
            $channel = new ChannelModel();
            $success = $channel->deleteAll($gid);
        }
        
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
}


