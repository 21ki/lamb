<?php

/*
 * The Group Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class GroupController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
    }

    public function indexAction() {
        $group = new GroupModel();
        $this->getView()->assign('groups', $group->getAll());

        return true;
    }

    public function createAction() {
        return false;
    }

    public function deleteAction() {
        return false;
    }

    public function updateAction() {
        return false;
    }
}
