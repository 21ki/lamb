<?php

/*
 * The Api Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ApiController extends Yaf\Controller_Abstract {
    public function init() {
        $this->request = $this->getRequest();
    }

    public function companyAction() {
        if ($this->request->isGet()) {
            $company = new CompanyModel();
            lambResponse(200, 'success', $company->get($this->request->getQuery('id'), null));
        }

        return false;
    }

    public function companysAction() {
        if ($this->request->isGet()) {
            $company = new CompanyModel();
            lambResponse(200, 'success', $company->getAll());
        }

        return false;
    }

    public function accountAction() {
        if ($this->request->isGet()) {
            $account = new AccountModel();
            lambResponse(200, 'success', $account->get($this->request->getQuery('id', null)));
        }

        return false;
    }
    
    public function accountsAction() {
        if ($this->request->isGet()) {
            $companys = [];
            $company = new CompanyModel();
            foreach ($company->getAll() as $c) {
                $companys[$c['id']] = $c;
            }

            $account = new AccountModel();
            $accounts = $account->getAll();

            foreach ($accounts as &$a) {
                unset($a['password']);
                $a['company'] = $companys[$a['company']]['name'];
            }

            lambResponse(200, 'success', $accounts);
        }

        return false;
    }
    
    public function routingAction() {
        if ($this->request->isGet()) {
            $routing = new RoutingModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $routing->get($this->request->getQuery('id', null));

            header('Content-type: application/json');
            echo json_encode($response);
        }
        

        return false;
    }

    public function routingsAction() {
        if ($this->request->isGet()) {
            $routing = new RoutingModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $routing->getAll();

            header('Content-type: application/json');
            echo json_encode($response);
        }
        return false;
    }

    public function channelAction() {
        if ($this->request->isGet()) {
            $channel = new ChannelModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $channel->get($this->request->getQuery('id', null), $this->request->getQuery('rid', null));

            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }

    public function gatewaysAction() {
        if ($this->request->isGet()) {
            $gateway = new GatewayModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $gateway->getAll();

            foreach ($response['data'] as &$g) {
                unset($g['password']);
            }
            
            header('Content-type: application/json');
            echo json_encode($response);
        }
        return false;
    }

    public function templateAction() {
        if ($this->request->isGet()) {
            $template = new TemplateModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $template->get($this->request->getQuery('id'));

            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }

    public function templatesAction() {
        if ($this->request->isGet()) {
            $template = new TemplateModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $template->getAll();

            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }

    public function groupAction() {
        $group = new GroupModel();
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $group->get($this->request->getQuery('id', null));

        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }

    public function groupsAction() {
        if ($this->request->isGet()) {
            $group = new GroupModel();
            $response['status'] = 200;
            $response['message'] = 'success';
            $response['data'] = $group->getAll();

            header('Content-type: application/json');
            echo json_encode($response);
        }

        return false;
    }
}
