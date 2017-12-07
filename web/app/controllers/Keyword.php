<?php

/*
 * The Keyword Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class KeywordController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }

    public function indexAction() {
        return true;
    }

    public function createAction() {
        if ($this->request->isPost()) {
            $keyword = new KeywordModel();
            $success = $keyword->create($this->request->getPost());
            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function tagsAction() {
        if ($this->request->isGet()) {
            $keyword = new KeywordModel();
            lambResponse(200, 'success', $keyword->getTags());
        }

        return false;
    }
    
    public function deleteAction() {
        return false;
    }
}
