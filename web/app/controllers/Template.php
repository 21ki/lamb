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

    public function summaryAction() {
        if ($this->request->isGet()) {
            $account = new AccountModel();
            $accounts = $account->getAll();

            $template = new TemplateModel();
            foreach ($accounts as &$a) {
                unset($a['password']);
                $a['total'] = $template->total($a['id']);
            }

            lambResponse(200, 'success', $accounts);
        }

        return false;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $template = new TemplateModel();
            $post = $this->request->getPost();
            if ($template->create($post)) {
                $account = new AccountModel();
                $account->signalNotification(intval($post['acc']));
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
            $id = $this->request->getQuery('id', 0);
            $post = $this->request->getPost();

            if ($template->change($id, $post)) {
                $t = $template->get($id);
                $account = new AccountModel();
                $account->signalNotification(intval($t['acc']));
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
            $id = $this->request->getQuery('id', 0);

            if ($template->isExist($id)) {
                $t = $template->get($id);
                if ($template->delete($id)) {

                    /* Send reload signal */
                    $account = new AccountModel();
                    $account->signalNotification($t['acc']);
                    lambResponse(200, 'success');
                    return false;
                }
            }
            lambResponse(400, 'failed');
        }

        return false;
    }

    public function cleanupAction() {
        if ($this->request->method == 'DELETE') {
            $success = false;
            $acc = $this->request->getQuery('id', null);

            if ($acc !== null) {
                $acc = intval($acc);
                $template = new TemplateModel();
                $success = $template->cleanup($acc);

                if ($success) {
                    $account = new AccountModel();
                    $account->signalNotification($acc);
                }
            }

            lambResponse($success ? 200 : 400, $success ? 'success' : 'failed');
        }

        return false;
    }

    public function templatesAction() {
        $acc = $this->request->getQuery('acc', 0);

        $account = new AccountModel();
        $acc = $account->get($acc);
        $this->getView()->assign('acc', isset($acc['id']) ? $acc['id'] : 0);
        $this->getView()->assign('name', isset($acc['username']) ? $acc['username'] : 'unknown');
        return true;
    }
}


