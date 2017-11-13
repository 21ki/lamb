<?php

/*
 * The Channel Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ChannelsController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
    }
    
    public function indexAction() {
        $gid = $this->request->getQuery('id', null);
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
            $data['id'] = $request->getPost('id', null);
            $data['gid'] = $request->getPost('gid', null);
            $data['weight'] = $request->getPost('weight', 1);
            $cmcc = $request->getPost('cmcc', null);
            $ctcc = $request->getPost('ctcc', null);
            $cucc = $request->getPost('cucc', null);
            $other = $request->getPost('other', null);
            $operator = 0;
            if ($cmcc === 'on') {$operator |= 1;}
            if ($ctcc === 'on') {$operator |= (1 << 1);}
            if ($cucc === 'on') {$operator |= (1 << 2);}
            if ($other === 'on') {$operator |= (1 << 3);}
            $data['operator'] = $operator;
            $group = new GroupModel();
            if ($group->isExist($data['gid'])) {
                $channel = new ChannelModel();
                $channel->create($data);
            }
            $response = $this->getResponse();
            $response->setRedirect('/channels?id=' . intval($data['gid']));
            $response->response();
        }
        return false;
    }
    public function deleteAction() {
        $success = false;
        $request = $this->getRequest();
        $channel = new ChannelModel();
        if ($channel->delete($request->getQuery('id', null), $request->getQuery('rid', null))) {
            $success = true;
        }
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
    public function updateAction() {
        $request = $this->getRequest();
        $gid = $request->getPost('gid', null);
        
        if ($request->isPost()) {
            $data['id'] = $request->getPost('id', null);
            $data['gid'] = $request->getPost('gid', null);
            $data['weight'] = $request->getPost('weight', null);
            $operator = 0;
            $operator |= $request->getPost('cmcc', false) ? 1 : 0;
            $operator |= $request->getPost('ctcc', false) ? (1 << 1) : 0;
            $operator |= $request->getPost('cucc', false) ? (1 << 2) : 0;
            $operator |= $request->getPost('other', false) ? (1 << 3) : 0;
            $data['operator'] = $operator;
            $channel = new ChannelModel();
            $channel->change($data);
        }
        $response = $this->getResponse();
        $response->setRedirect('/channels?id=' . intval($gid));
        $response->response();
        return false;
    }
}
