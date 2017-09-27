<?php

/*
 * The Channel Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

class ChannelController extends Yaf\Controller_Abstract {
    public function createAction() {
        $request = $this->getRequest();

        if ($request->isPost()) {
            $data['id'] = $request->getPost('id', null);
            $data['gid'] = $request->getPost('gid', null);
            $data['weight'] = $request->getPost('weight', 1);
            $cmcc = $request->getPost('cmcc', null);
            $ctcc = $request->getPost('ctcc', null);
            $cucc = $request->getPost('cucc', null);
            $other = $request->getPost('other', null);

            $operator = 0;
            if ($cmcc === 'on') {$operator |= 1;}
            if ($ctcc === 'on') {$operator |= (1 << 1);}
            if ($cucc === 'on') {$operator |= (1 << 2);}
            if ($other === 'on') {$operator |= (1 << 3);}

            $data['operator'] = $operator;

            $group = new GroupModel();

            if ($group->isExist($data['gid'])) {
                $channel = new ChannelModel();
                $channel->create($data);
            }

            $response = $this->getResponse();
            $response->setRedirect('/group?id=' . intval($data['gid']));
            $response->response();
        }

        return false;
    }

    public function deleteAction() {
        $success = false;
        $request = $this->getRequest();

        $channel = new ChannelModel();
        if ($channel->delete($request->getQuery('id', null), $request->getQuery('gid', null))) {
            $success = true;
        }

        $response['status'] = $success ? 200 : 400;
        $response['message'] = $success ? 'success' : 'failed';
        header('Content-type: application/json');
        echo json_encode($response);

        return false;
    }
}


