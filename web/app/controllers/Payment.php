<?php

/*
 * The Payment Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class PaymentController extends Yaf\Controller_Abstract {
    public function indexAction() {
        return true;
    }

    public function logsAction() {
        $payment = new PaymentModel();
        $this->getView()->assign('data', $payment->getLogs());
        return true;
    }
}


