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
        $company = new CompanyModel();

        $list = $company->getAll();
        foreach ($list as &$val) {
            $val['money'] = $company->getMoney($val['id']);
            $val['count'] = $company->accountTotal($val['id']);
        }

        $this->getView()->assign('companys', $list);
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();
        if ($request->isPost()) {
            $company = new CompanyModel();
            $company->create($request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/company');
            $response->response();
            return false;
        }

        return true;
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
        $request = $this->getRequest();

        $company = new CompanyModel();
        $success = $company->delete($request->getQuery('id'));
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }

    public function rechargeAction() {
        if ($this->request->isPost()) {
            $company = new CompanyModel();
            $success = $company->recharge($this->request->getQuery('id', null), $this->request->getPost('money'));
            $response['status'] = $success ? 200 : 400;
            $response['message'] = $success ? 'success' : 'failed';
            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }

    public function isExist($id = null) {
        $result = false;
        $sql = 'SELECT count(id) FROM ' . $this->table . ' WHERE id = ' . intval($id) . ' LIMIT 1';
        $sth = $this->db->query($sql);
        if ($sth && ($result = $sth->fetch()) !== false) {
            if ($result['count'] > 0) {
                return true;
            }
        }

        return false;
    }
}


