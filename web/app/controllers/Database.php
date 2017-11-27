<?php

/*
 * The Database Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class DatabaseController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
    }
    
    public function indexAction() {
        $database = new DatabaseModel();
        $this->getView()->assign('databases', $database->getAll());
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $database = new DatabaseModel();
            $database->create($this->request->getPost());
        }

        $this->response->setRedirect('/database');
        $this->response->response();
        return false;
    }

    public function changeAction() {
        $this->response->setRedirect('/database');
        $this->response->response();
        return false;

    }
    
    public function deleteAction() {
        $this->response->setRedirect('/database');
        $this->response->response();
        return false;
    }
}
