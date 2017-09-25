<?php

/*
 * The Routing Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class RoutingController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $request = $this->getRequest();
        $group = new GroupModel();
        $groups = $group->getAll();

        $channel = new ChannelModel();
        foreach ($groups as &$g) {
            $g['total'] = $channel->getTotal($g['id']);
        }

        $gateway = new GatewayModel();
        
        $this->getView()->assign('groups', $group->getAll());
        $this->getView()->assign('gateways', $gateway->getAll());

        return true;
    }
}


