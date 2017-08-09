<?php

/*
 * The Content Filter
 * Link http://github.com/typefo/lamb
 * Copyright (C) typefo <typefo@qq.com>
 */

namespace Tool;

class Filter {

    static public function alpha($value = null, $defval = null, $min = 0, $max = 4096) {
        $value = filter_var($value, FILTER_SANITIZE_STRING);
        if ($value) {
            $value = preg_replace('/[^a-zA-Z0-9]/', '', $value);
            $len = mb_strlen($value);
            if ($len >= $min && $len <= $max) {
                return $value;
            }
        }

        return $defval;
    }

    static public function number($value = null, $defval = null, $min = 0, $max = PHP_INT_MAX) {
        $value = intval($value);
        if ($value >= $min && $value <= $max) {
            return $value;
        }

        return $defval;
    }

    static public function string($value = null, $defval = null, $min = 0, $max = 4096) {
        $value = filter_var($value, FILTER_SANITIZE_STRING);
        if ($value) {
            $len = mb_strlen($value);
            if ($len >= $min && $len <= $max) {
                return $value;
            }
        }

        return $defval;
    }
    
    static public function dateTime($value = null, $defval = null) {
        if (is_string($value)) {
            $format = 'Y-m-d H:i:s';
            $d = \DateTime::createFromFormat($format, $value);
            if ($d && $d->format($format) === $value) {
                return $value;
            }
        }

        return $defval;
    }

    static public function ip($value = null, $defval = null) {
        $value = filter_var($value, FILTER_VALIDATE_IP);
        if ($value) {
            return $value;
        }

        return $defval;
    }

    static public function port($value = null, $defval = null) {
        $value = intval($value);
        if ($value >= 0 && $value <= 65535) {
            return $value;
        }

        return $defval;
    }

    static public function email($value = null, $defval = null) {
        $value = filter_var($value, FILTER_VALIDATE_EMAIL);;
        if ($value) {
            return $value;
        }

        return $defval;
    }
}
