<?php

/*
 * The Company Controller
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

use Tool\Filter;

class CompanyController extends Yaf\Controller_Abstract {
    public function indexAction() {
        $company = new CompanyModel();
        $this->getView()->assign('data', $company->getAll());
        return true;
    }

    public function createAction() {
        return true;
    }

    public function editAction() {
        $request = $this->getRequest();
        $company = new CompanyModel();
        $this->getView()->assign('company', $company->get($request->getQuery('id')));
        return true;
    }
}


