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
        $account = new AccountModel();
        $accounts = [];
        foreach ($account->getAll() as $val) {
            $accounts[$val['id']] = $val['username'];
        }

        $this->getView()->assign('accounts', $accounts);
        $this->getView()->assign('templates', $template->getAll());
        
        return true;
    }

    public function createAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $data = $request->getPost();
            $data['description'] = 'no description';
            $template = new TemplateModel();
            $template->create($data);
        }

        $response = $this->getResponse();
        $response->setRedirect('/template');
        $response->response();

        return false;
    }

    public function updateAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $template = new TemplateModel();
            $id = $request->getPost("id", null);
            $template->change($id, $request->getPost());
        }

        $response = $this->getResponse();
        $response->setRedirect('/template');
        $response->response();
        return false;
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


