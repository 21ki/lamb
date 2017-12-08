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

    public function elementsAction() {
        $tag = $this->request->getQuery('tag', '');
        $this->getView()->assign('tag', $tag);
        return true;
    }

    public function getkeywordsAction() {
        if ($this->request->isGet()) {
            $keyword = new KeywordModel();
            $tag = $this->request->getQuery('tag', null);
            lambResponse(200, 'success', $keyword->getClassification(urldecode($tag)));
        }

        return false;
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
        if ($this->request->method == 'DELETE') {
            $id = $this->request->getQuery('id', null);
            $tag = $this->request->getQuery('tag', null);
            $keyword = new KeywordModel();
            if ($id !== null) {
                $keyword->delete($id);
            } else if ($tag !== null) {
                $keyword->deleteClassification($tag);
            }

            lambResponse(200, 'success');
        }

        return false;
    }
}
