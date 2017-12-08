<?php

/*
 * The Company Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Db\Redis;
use Tool\Filter;

class CompanyController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }

    public function indexAction() {
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $company = new CompanyModel();
            $success = $company->create($this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $company = new CompanyModel();
            $success = $company->change($this->request->getQuery('id', null), $this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $company = new CompanyModel();
            $success = $company->delete($this->request->getQuery('id'));
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function rechargeAction() {
        if ($this->request->isPost()) {
            $company = new CompanyModel();
            $id = $this->request->getQuery('id', 0);
            $money = $this->request->getPost('money', 0);
            $description = $this->request->getPost('description', 'no description');
            $success = $company->recharge($id, $money, $description);
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function isExist($id = null) {
        $id = intval($id);
        $result = false;
        $sql = 'SELECT count(id) FROM ' . $this->table . ' WHERE id = ' . $id . ' LIMIT 1';
        $sth = $this->db->query($sql);
        if ($sth) {
            $result = $sth->fetch();
            if ($result['count'] > 0) {
                return true;
            }
        }

        return false;
    }
}

