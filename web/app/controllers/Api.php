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
            lambResponse(200, 'success', $routing->getAll());
        }

        return false;
    }

    public function channelAction() {
        if ($this->request->isGet()) {
            $c = new ChannelModel();
            $id = $this->request->getQuery('id', null);
            $gid = $this->request->getQuery('gid', null);
            $channel = $c->get($id, $gid);

            $g = new GatewayModel();
            $gateway = $g->get($channel['id']);
            $channel['name'] = $gateway['name'];
            lambResponse(200, 'success', $channel);
        }

        return false;
    }
    
    public function channelsAction() {
        if ($this->request->isGet()) {
            $gateway = new GatewayModel();
            $gateways = [];

            foreach ($gateway->getAll() as $g) {
                $gateways[$g['id']] = $g;
            }

            $channel = new ChannelModel();
            $channels = $channel->getAll($this->request->getQuery('gid', null));
            foreach ($channels as &$chan) {
                $chan['name'] = $gateways[$chan['id']]['name'];
            }
            
            lambResponse(200, 'success', $channels);
        }

        return false;
    }

    public function gatewayAction() {
        if ($this->request->isGet()) {
            $gateway = new GatewayModel();
            lambResponse(200, 'success', $gateway->get($this->request->getQuery('id', null)));
        }

        return false;
    }

    public function gatewaysAction() {
        if ($this->request->isGet()) {
            $gateway = new GatewayModel();
            $gateways = $gateway->getAll();

            foreach ($gateways as &$g) {
                unset($g['password']);
            }

            lambResponse(200, 'success', $gateways);
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
            $acc = $this->request->getQuery('acc');
            $template = new TemplateModel();
            $templates = $template->getAll(intval($acc));
            lambResponse(200, 'success', $templates);
        }

        return false;
    }

    public function groupAction() {
        if ($this->request->isGet()) {
            $group = new GroupModel();
            lambResponse(200, 'success', $group->get($this->request->getQuery('id', null)));
        }
        
        return false;
    }

    public function groupsAction() {
        if ($this->request->isGet()) {
            $group = new GroupModel();
            lambResponse(200, 'success' ,$group->getAll());
        }

        return false;
    }

    public function deliveryAction() {
        if ($this->request->isGet()) {
            $delivery = new DeliveryModel();
            lambResponse(200, 'success', $delivery->get($this->request->getQuery('id', null)));
        }

        return false;
    }
    
    public function deliverysAction() {
        if ($this->request->isGet()) {
            $account = new AccountModel();
            $accounts = [];
            foreach ($account->getAll() as $a) {
                $accounts[$a['id']] = $a;
            }

            $delivery = new DeliveryModel();
            $deliverys = $delivery->getAll();
            foreach ($deliverys as &$d) {
                $d['target'] = $accounts[$d['target']]['username'];
            }

            lambResponse(200, 'success', $deliverys);
        }

        return false;
    }

    public function keywordsAction() {
        if ($this->request->isGet()) {
            $keyword = new KeywordModel();
            $limit = $this->request->getQuery('limit', null);
            lambResponse(200, 'success', $keyword->getAllClassification($limit));
        }

        return false;
    }

    public function inboundAction() {
        $company = new CompanyModel();
        $companys = [];

        foreach($company->getAll() as $c) {
            $companys[$c['id']] = $c;
        }
        
        $status = new StatusModel();
        $accounts = $status->inbound();
        foreach ($accounts as &$a) {
            $a['company'] = $companys[$a['company']]['name'];
        }
        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $accounts;

        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }

    public function outboundAction() {
        $status = new StatusModel();

        $response['status'] = 200;
        $response['message'] = 'success';
        $response['data'] = $status->outbound();

        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }

    public function checkMessagesAction() {
        $gateway = new GatewayModel();
        $response['status'] = 200;
        $response['message'] = 'success';

        $gateways = [];
        foreach ($gateway->getAll() as $g) {
            $gateways[$g['id']] = $g;
        }

        $messages = $gateway->fetchCheckMessages();
        foreach ($messages as &$m) {
            $m['channel'] = $gateways[$m['channel']]['name'];
        }

        $response['data'] = $messages;

        header('Content-type: application/json');
        echo json_encode($response);
        return false;
    }
}
