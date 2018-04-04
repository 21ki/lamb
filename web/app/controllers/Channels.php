<?php

/*
 * The Channel Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ChannelsController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }
    
    public function indexAction() {
        $this->getView()->assign('acc', $this->request->getQuery('id', null));
        return true;
    }
    
    public function createAction() {
        if ($this->request->isPost()) {
            $channel = new ChannelModel();
            $post = $this->request->getPost();
            $success = $channel->create($post);

            if ($success) {
                $acc = isset($post['acc']) ? $post['acc'] : null;
                $account = new AccountModel();
                $account->signalNotification(intval($acc));
            }

            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }
    public function deleteAction() {
        if ($this->request->method == 'DELETE') {
            $channel = new ChannelModel();
            $id = $this->request->getQuery('id', null);
            $acc = $this->request->getQuery('acc', null);
            $success = $channel->delete($id, $acc);

            if ($success) {
                $acc = intval($acc);
                $account = new AccountModel();
                $account->signalNotification($acc);
            }

            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }

    public function updateAction() {
        if ($this->request->isPost()) {
            $channel = new ChannelModel();
            $id = $this->request->getQuery('id', null);
            $acc = $this->request->getQuery('acc', null);
            $success = $channel->change($id, $acc, $this->request->getPost());

            if ($success) {
                $acc = intval($acc);
                $account = new AccountModel();
                $account->signalNotification($acc);
            }

            $status = $success ? 200 : 400;
            $message = $success ? 'success' : 'failed';
            lambResponse($status, $message);
        }

        return false;
    }
}
