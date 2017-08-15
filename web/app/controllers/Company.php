<?php

/*
 * The Company Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class CompanyController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $company = new CompanyModel();
        $this->getView()->assign('data', $company->getAll());
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();
        if ($request->isPost()) {
            $company = new CompanyModel();
            $company->create($request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/company';
            $response = $this->getResponse();
            $response->setRedirect($url);
            $response->response();
            return false;
        }

        return true;
    }

    public function editAction() {
        $request = $this->getRequest();
        $company = new CompanyModel();

        if ($request->isPost()) {
            $company->change($request->getPost('id'), $request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/company';
            $response = $this->getResponse();
            $response->setRedirect($url);
            $response->response();
            return false;
        }

        $this->getView()->assign('company', $company->get($request->getQuery('id')));
        return true;
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
        $request = $this->getRequest();

        if ($request->isPost()) {
            $company = new CompanyModel();
            $success = $company->recharge($request->getPost('id'), $request->getPost('money'));
            $response['status'] = $success ? 200 : 400;
            $response['message'] = $success ? 'success' : 'failed';
            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }
}


