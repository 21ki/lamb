<?php

/*
 * The Common Funcions
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

function lamb_output($val = null, $len = -1) {
    if ($len > 0) {
        return htmlspecialchars(mb_substr($val, 0, $len, 'UTF-8'), ENT_QUOTES);
    }
    
    return htmlspecialchars($val, ENT_QUOTES);
}

function str2int(&$val) {
    $val = intval($val);
}

function lambResponse(int $code = 200, string $message = "success") {
    header('Content-type: application/json');
    echo json_encode(['status' => $code, 'message' => $message]);
}

