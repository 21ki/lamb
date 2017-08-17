<?php

/*
 * The Security Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class SecurityController extends Yaf\Controller_Abstract {
    public function blacklistAction() {
        $security = new SecurityModel();
        $this->getView()->assign('blacklist', $security->blacklist());
        return true;
    }

    public function whitelistAction() {
        $security = new SecurityModel();
        $this->getView()->assign('whitelist', $security->whitelist());
        return true;
    }

    public function deleteAction() {
        $request = $this->getRequest();

        $type = $request->getQuery('type', null);
        $security = new SecurityModel();
        switch ($type) {
        case 'blacklist':
            $security->delete('blacklist', $request->getQuery('phone'));
            break;
        case 'whitelist':
            $security->delete('whitelist', $request->getQuery('phone'));
            break;
        }
        
        return false;
    }
}
