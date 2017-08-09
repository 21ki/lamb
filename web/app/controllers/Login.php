<?php

/*
 * The Login Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class LoginController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $request = $this->getRequest();
        $response = $this->getResponse();
        $url = 'http://' . $_SERVER['SERVER_ADDR'] . ':' . $_SERVER['SERVER_PORT'];

        /* Check login action */
        if ($request->isPost()) {
            $data = $request->getPost();
            $login = new LoginModel($data);

            /* Verify username and password */
            if (!$login->verify()) {
                goto output;
            }

            /* Check ip whitelist */
            $config = new ConfigModel();
            if ($config->get('security') === '1') {
                $acl = new AclModel();
                if (!$acl->check($_SERVER['REMOTE_ADDR'])) {
                    goto output;
                }
            }

            $session = Yaf\Session::getInstance();
            $session->set('login', true);
            $response->setRedirect($url . '/cdr');
            $response->response();
            return false;
        }

        output:
        $response->setRedirect($url);
        $response->response();
        return false;
    }
}


