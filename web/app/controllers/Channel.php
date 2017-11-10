<?php

/*
 * The Channel Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class ChannelController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $route = new RoutingModel();
        $this->getView()->assign('routes', $route->getAll());
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();
        $account = new AccountModel();

        if ($request->isPost()) {
            $route = new RoutingModel();
            $route->create($request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/routing');
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
        $route = new RoutingModel();

        if ($request->isPost()) {
            $route->change($request->getPost('id'), $request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/routing');
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
        $route = new RoutingModel();
        $success = $route->delete($request->getQuery('id'));
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
}


