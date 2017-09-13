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

        /* Check login action */
        if ($request->isPost()) {
            $data = $request->getPost();
            $login = new LoginModel($data);

            /* Verify username and password */
            if (!$login->verify()) {
                goto output;
            }

            $session = Yaf\Session::getInstance();
            $session->set('login', true);
            $response->setRedirect($url . '/status/inbound');
            $response->response();
            return false;
        }

        output:
        $response->setRedirect('/');
        $response->response();
        return false;
    }
}


