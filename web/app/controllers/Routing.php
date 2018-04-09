<?php

/*
 * The Routing Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class RoutingController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }

    public function indexAction() {
        return true;
    }

    public function summaryAction() {
        if ($this->request->isGet()) {
            $account = new AccountModel();
            $accounts = $account->getAll();

            $channel = new ChannelModel();
            foreach ($accounts as &$a) {
                unset($a['password']);
                $a['total'] = $channel->total($a['id']);
            }

            lambResponse(200, 'success', $accounts);
        }

        return false;
    }

    public function cleanupAction() {
        if ($this->request->method == 'DELETE') {
            $success = false;
            $channel = new ChannelModel();
            $acc = $this->request->getQuery('id', null);

            if ($acc !== null) {
                $acc = intval($acc);
                $success = $channel->cleanup($acc);

                if ($success) {
                    $account = new AccountModel();
                    $account->signalNotification($acc);
                }
            }

            lambResponse($success ? 200 : 400, $success ? 'success' : 'failed');
        }

        return false;
    }
}
