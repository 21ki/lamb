<?php

/*
 * The Routes Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class RoutesController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $request = $this->getRequest();
        $route = new RouteModel();
        $this->getView()->assign('routes', $route->getAll());
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();
        $account = new AccountModel();

        if ($request->isPost()) {
            $route = new RouteModel();
            $route->create($request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/routes';
            $response = $this->getResponse();
            $response->setRedirect($url);
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
        $route = new RouteModel();

        if ($request->isPost()) {
            $route->change($request->getPost('id'), $request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/routes';
            $response = $this->getResponse();
            $response->setRedirect($url);
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
        $route = new RouteModel();
        $success = $route->delete($request->getQuery('id'));
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
}


