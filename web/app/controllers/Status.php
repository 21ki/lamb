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

        $status = new StatusModel();
        $this->getView()->assign('clients', $status->inbound($account->getAll()));

        return true;
    }

    public function outboundAction() {
        $gateway = new GatewayModel();

        $status = new StatusModel();
        $this->getView()->assign('gateway', $status->outbound($gateway->getAll()));

        return true;
    }
}


