<?php

/*
 * The Delivery Route Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class DeliveryController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
    }
    
    public function indexAction() {
        $delivery = new DeliveryModel();
        $account = new AccountModel();

        $accounts = [];
        foreach ($account->getAll() as $a) {
            $accounts[$a['id']] = $a;
        }
        
        $this->getView()->assign('deliverys', $delivery->getAll());
        $this->getView()->assign('accounts', $accounts);
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();
        $account = new AccountModel();

        if ($request->isPost()) {
            $route = new DeliveryModel();
            $route->create($request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/delivery');
            $response->response();
            return false;
        }

        $company = new CompanyModel();
        $list = array();
        foreach ($company->getAll() as $key => $val) {
            $list[$val['id']] = $val;
        }

        $this->getView()->assign('companys', $list);
        $this->getView()->assign('accounts', $account->getAll());
        return true;
    }

    public function editAction() {
        $request = $this->getRequest();
        $route = new DeliveryModel();

        if ($request->isPost()) {
            $route->change($request->getPost('id'), $request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/delivery');
            $response->response();
            return false;
        }

        $company = new CompanyModel();
        $list = array();
        foreach ($company->getAll() as $key => $val) {
            $list[$val['id']] = $val;
        }

        $this->getView()->assign('companys', $list);
        
        $account = new AccountModel();
        $this->getView()->assign('accounts', $account->getAll());
        $this->getView()->assign('route', $route->get($request->getQuery('id', null)));
        return true;
    }

    public function deleteAction() {
        $request = $this->getRequest();
        $route = new DeliveryModel();
        $success = $route->delete($request->getQuery('id'));
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
}
