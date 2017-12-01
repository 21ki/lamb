<?php

/*
 * The Group Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class GroupsController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
        $this->group = new GroupModel();
    }

    public function indexAction() {
        $this->getView()->assign('groups', $this->group->getAll());

        return true;
    }

    public function createAction() {
        $this->group->create($this->request->getPost());
        $this->response->setRedirect('/groups');
        $this->response->response();
        return false;
    }

    public function deleteAction() {
        $this->group->delete($this->request->getQuery('id', null));
        return false;
    }

    public function changeAction() {
        $this->group->change($this->request->getPost('id', null),
                             $this->request->getPost());
        $this->response->setRedirect('/groups');
        $this->response->response();

        return false;
    }
}
