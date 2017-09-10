<?php

/*
 * The Gateway Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class GatewayController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $channel = new GatewayModel();
        $this->getView()->assign('channels', $channel->getAll());
        return true;
    }

    public function createAction() {
        $request =  $this->getRequest();

        if ($request->isPost()) {
            $channel = new GatewayModel();
            $channel->create($request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/channel';
            $response = $this->getResponse();
            $response->setRedirect($url);
            $response->response();
            return false;
            
        }
        
        return true;
    }

    public function editAction() {
        $request = $this->getRequest();
        $channel = new GatewayModel();

        if ($request->isPost()) {
            $channel->change($request->getPost('id'), $request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/channel';
            $response = $this->getResponse();
            $response->setRedirect($url);
            $response->response();
            return false;
        }

        $this->getView()->assign('channel', $channel->get($request->getQuery('id')));
        return true;
    }

    public function deleteAction() {
        $request = $this->getRequest();
        
    }

    public function testAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $channel = new GatewayModel();
            $channel->test($request->getPost('id'), $request->getPost('phone'), $request->getPost('message'));
        }

        $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/channel';
        $response = $this->getResponse();
        $response->setRedirect($url);
        $response->response();

        return false;
    }
    
    public function groupsAction() {
        $request = $this->getRequest();
        $channel = new GatewayModel();
        $group = new GroupModel();
        
        if ($request->isPost()) {
            $op = $request->getQuery('op', null);
            switch ($op) {
            case 'create':
                echo '<pre>';
                var_dump($group->create($request->getPost(), $request->getPost('channels'), $request->getPost('weights')));
                echo '</pre>';
                exit;
                break;
            case 'delete':
                $group->delete($request->getPost('id'));
                break;
            case 'edit':
                $group->change($request->getPost('id'), $request->getPost());
                break;
            }
        }

        $this->getView()->assign('channels', $channel->getAll());
        $this->getView()->assign('groups', $group->getAll());
        return true;
    }
}


