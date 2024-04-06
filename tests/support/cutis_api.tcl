#!/usr/bin/tclsh
#
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com
#

source "support/cutis_lowlevel_api.tcl"

### Actual API ###
proc cutis_set {fd key val} {
    cutis_writenl $fd "set $key [string length $val]\r\n$val"
    cutis_read_retcode $fd
}

proc cutis_setnx {fd key val} {
    cutis_writenl $fd "setnx $key [string length $val]\r\n$val"
    cutis_read_retcode $fd
}

proc cutis_get {fd key} {
    cutis_writenl $fd "get $key"
    cutis_bulk_read $fd
}

proc cutis_select {fd id} {
    cutis_writenl $fd "select $id"
    cutis_read_retcode $fd
}

proc cutis_move {fd key id} {
    cutis_writenl $fd "move $key $id"
    cutis_read_retcode $fd
}

proc cutis_del {fd key} {
    cutis_writenl $fd "del $key"
    cutis_read_retcode $fd
}

proc cutis_keys {fd pattern} {
    cutis_writenl $fd "keys $pattern"
    split [cutis_bulk_read $fd]
}

proc cutis_dbsize {fd} {
    cutis_writenl $fd "dbsize"
    cutis_read_integer $fd
}

proc cutis_incr {fd key} {
    cutis_writenl $fd "incr $key"
    cutis_read_integer $fd
}

proc cutis_decr {fd key} {
    cutis_writenl $fd "decr $key"
    cutis_read_integer $fd
}

proc cutis_exists {fd key} {
    cutis_writenl $fd "exists $key"
    cutis_read_integer $fd
}

proc cutis_lpush {fd key val} {
    cutis_writenl $fd "lpush $key [string length $val]\r\n$val"
    cutis_read_retcode $fd
}

proc cutis_rpush {fd key val} {
    cutis_writenl $fd "rpush $key [string length $val]\r\n$val"
    cutis_read_retcode $fd
}

proc cutis_llen {fd key} {
    cutis_writenl $fd "llen $key"
    cutis_read_integer $fd
}

proc cutis_lindex {fd key index} {
    cutis_writenl $fd "lindex $key $index"
    cutis_bulk_read $fd
}

proc cutis_lrange {fd key first last} {
    cutis_writenl $fd "lrange $key $first $last"
    cutis_multi_bulk_read $fd
}

proc cutis_ltrim {fd key first last} {
    cutis_writenl $fd "ltrim $key $first $last"
    cutis_read_retcode $fd
}

proc cutis_rename {fd key1 key2} {
    cutis_writenl $fd "rename $key1 $key2"
    cutis_read_retcode $fd
}

proc cutis_renamenx {fd key1 key2} {
    cutis_writenl $fd "renamenx $key1 $key2"
    cutis_read_retcode $fd
}

proc cutis_lpop {fd key} {
    cutis_writenl $fd "lpop $key"
    cutis_bulk_read $fd
}

proc cutis_rpop {fd key} {
    cutis_writenl $fd "rpop $key"
    cutis_bulk_read $fd
}

