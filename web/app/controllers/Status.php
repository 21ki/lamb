<?php

/*
 * The Status Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class StatusController extends Yaf\Controller_Abstract {
    public function inboundAction() {
        return true;
    }

    public function outboundAction() {
        return true;
    }
}


