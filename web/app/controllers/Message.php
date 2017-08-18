<?php

/*
 * The Message Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class MessageController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $request = $this->getRequest();
        $message = new MessageModel();

        if ($request->getQuery('sub', null) !== null) {

        }

        $this->getView()->assign('messages', $message->getAll());
        return true;
    }

    public function deliverAction() {
        $request = $this->getRequest();
        $deliver = new DeliverModel();

        if ($request->getQuery('sub', null) !== null) {

        }

        $this->getView()->assign('delivers', $deliver->getAll());
        return true;
    }

    public function reportAction() {
        $request = $this->getRequest();
        $report = new ReportModel();
        if ($request->getQuery('query', null) !== null) {

        }

        $this->getView()->assign('reports', $report->getAll());
        return true;
    }
}


