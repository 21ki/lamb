<?php

/*
 * The Template Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class TemplateController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
        $this->response = $this->getResponse();
    }
    
    public function indexAction() {
        $template = new TemplateModel();
        $this->getView()->assign('templates', $template->getAll());
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $template = new TemplateModel();
            $success = $template->create($this->request->getPost());
            $response['status'] = $success ? 201 : 400;
            $response['message'] = $success ? 'success' : 'failed';
            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }

    public function changeAction() {
        if ($this->request->isPost()) {
            $template = new TemplateModel();
            $template->change($this->request->getPost('id', null), $this->request->getPost());
        }

        $this->response->setRedirect('/template');
        $this->response->response();
        return false;
    }

    public function deleteAction() {
        $template = new TemplateModel();
        $template->delete($this->request->getQuery('id', 0));
        $this->response->setRedirect('/template');
        $this->response->response();
        return false;
    }
}


