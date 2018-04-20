<?php

/*
 * The Service Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ServiceController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }

    public function coreAction() {
        return true;
    }

    public function serverAction() {
        return true;
    }

    public function gatewayAction() {
        return true;
    }

    public function mtAction() {
        return true;
    }

    public function moAction() {
        return true;
    }

    public function getcoreAction() {
        if ($this->request->isGet()) {
            $service = new ServiceModel();
            lambResponse(200, 'success', $service->core());
        }

        return false;
    }

    public function getqueueAction() {
        if ($this->request->isGet()) {
            $type = $this->request->getQuery('type', null);
            if ($type !== null) {
                $service = new ServiceModel();
                lambResponse(200, 'success', $service->queue($type));
            }
        }

        return false;
    }

    public function getserverAction() {
        if ($this->request->isGet()) {
            $service = new ServiceModel();
            lambResponse(200, 'success', $service->server());
        }

        return false;
    }
}


