<?php

/*
 * The Account Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class AccountController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $company = new CompanyModel();
        $list = array();
        foreach ($company->getAll() as $key => $val) {
            $list[$val['id']] = $val;
        }
        $account = new AccountModel();
        $this->getView()->assign('companys', $list);
        $this->getView()->assign('accounts', $account->getAll());
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $account = new AccountModel();
            $account->create($request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/account');
            $response->response();
            return false;
        }

        $company = new CompanyModel();
        $list = $company->getAll();
        $this->getView()->assign('companys', $list);
        
        $group = new GroupModel();
        $this->getView()->assign('groups', $group->getAll());

        return true;
    }

    public function editAction() {
        $request = $this->getRequest();
        $account = new AccountModel();

        if ($request->isPost()) {
            $account->change($request->getPost('id'), $request->getPost());
            $response = $this->getResponse();
            $response->setRedirect('/account');
            $response->response();
            return false;
        }

        $group = new GroupModel();
        $company = new CompanyModel();
        $this->getView()->assign('companys', $company->getAll());
        $this->getView()->assign('groups', $group->getAll());
        $this->getView()->assign('account', $account->get($request->getQuery('id')));

        return true;
    }


    public function deleteAction() {
        $request = $this->getRequest();
        $account = new AccountModel();
        $success = $account->delete($request->getQuery('id'));
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
}
