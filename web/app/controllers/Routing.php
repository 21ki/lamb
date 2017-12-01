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
        $this->routing = new RoutingModel();
    }

    public function indexAction() {
        $group = new GroupModel();

        $groups = [];
        foreach ($group->getAll() as $g) {
            $groups[$g['id']] = $g;
        }
        
        $this->getView()->assign('routings', $this->routing->getAll());
        $this->getView()->assign('groups', $groups);

        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $this->routing->create($this->request->getPost());
        }

        $this->response->setRedirect('/routing');
        $this->response->response();

        return false;
    }

    public function deleteAction() {
        $this->routing->delete($this->request->getQuery('id', null));
        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $id = $this->request->getPost('id', null);
            $this->routing->change($id, $this->request->getPost());
        }

        $this->response->setRedirect('/routing');
        $this->response->response();

        return false;
    }
}
