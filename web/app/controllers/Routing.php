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

        foreach ($groups as &$g) {
            $g['total'] = $group->total($g['id']);
        }

        $gateway = new GatewayModel();
        
        $this->getView()->assign('groups', $groups);
        $this->getView()->assign('gateways', $gateway->getAll());

        return true;
    }

    public function createAction() {
        $name = $this->getRequest()->getPost('name');
        $data = $this->getRequest()->getPost('channels');

        $channels = $this->duplicate_removal($data);

        $group = new GroupModel();
        $gid = $group->create(['name' => $name, 'description' => 'no description']);

        if ($gid > 0) {
            $channel = new ChannelModel();
            foreach ($channels as $chan) {
                $chan['gid'] = $gid;
                $chan['operator'] = 1 | (1 << 1) | (1 << 2);
                $channel->create($chan);
            }
        }

        $response = $this->getResponse();
        $response->setRedirect('/routing');
        $response->response();

        return false;
    }

    private function duplicate_removal(array $data = null) {
        $result = [];
        
        foreach ($data as $v1) {
            $have = true;
            foreach ($result as $v2) {
                if ($v1['id'] == $v2['id']) {
                    $have = false;
                }
            }

            if ($have) {
                $result[] = $v1;
            }
        }

        return $result;
    }
}


