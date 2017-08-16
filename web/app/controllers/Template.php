<?php

/*
 * The Template Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class TemplateController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $template = new TemplateModel();
        $this->getView()->assign('templates', $template->getAll());
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();
        $account = new AccountModel();

        if ($request->isPost()) {
            $template = new TemplateModel();
            $template->create($request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/template';
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
        $template = new TemplateModel();

        if ($request->isPost()) {
            $template->change($request->getPost('id'), $request->getPost());
            $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'] . '/template';
            $response = $this->getResponse();
            $response->setRedirect($url);
            $response->response();
            return false;
        }

        $company = new CompanyModel();
        $clist = array();
        foreach ($company->getAll() as $key => $val) {
            $clist[$val['id']] = $val;
        }

        $account = new AccountModel();
        $alist = array();
        foreach ($account->getAll() as $key => $val) {
            $alist[$val['id']] = $val;
        }

        $this->getView()->assign('companys', $clist);
        $this->getView()->assign('accounts', $alist);
        $this->getView()->assign('template', $template->get($request->getQuery('id')));
        return true;
    }

    public function deleteAction() {
        $request = $this->getRequest();
        $template = new TemplateModel();
        $success = $template->delete($request->getQuery('id'));
        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
}


