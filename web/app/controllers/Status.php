<?php

/*
 * The Status Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class StatusController extends Yaf\Controller_Abstract {
    public function inboundAction() {
        $account = new AccountModel();
        $data = array();

        $status = new StatusModel();
        $this->getView()->assign('accounts', $status->online($account->getAll()));

        return true;
    }

    public function outboundAction() {
        return true;
    }
}


