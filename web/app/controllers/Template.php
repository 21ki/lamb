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
            if ($template->create($this->request->getPost())) {
                lambResponse(200, 'success');
            } else {
                lambResponse(400, 'failed');
            }
        }

        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $template = new TemplateModel();
            if ($template->change($this->request->getQuery('id', null), $this->request->getPost())) {
                lambResponse(200, 'success');
            } else {
                lambResponse(400, 'failed');
            }
            
        }

        return false;
    }

    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $template = new TemplateModel();
            if ($template->delete($this->request->getQuery('id', null))) {
                lambResponse(200, 'success');
            } else {
                lambResponse(400, 'failed');
            }
        }

        return false;
    }
}


