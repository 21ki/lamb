<?php

/*
 * The Account Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class AccountController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }
    
    public function indexAction() {
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $account = new AccountModel();
            $success = $account->create($this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $account = new AccountModel();
            $account->change($this->request->getQuery('id', null), $this->request->getPost());
            lambResponse(200, 'success');
        }

        return false;
    }


    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $account = new AccountModel();
            $success = $account->delete($this->request->getQuery('id', null));
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }
}
