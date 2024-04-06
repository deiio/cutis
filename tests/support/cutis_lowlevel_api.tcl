#!/usr/bin/tclsh
#
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com
#

proc cutis_connect {server port} {
    set fd [socket $server $port]
    fconfigure $fd -translation binary
    return $fd
}

proc cutis_write {fd buf} {
    puts -nonewline $fd $buf
}

proc cutis_writenl {fd buf} {
    cutis_write $fd $buf
    cutis_write $fd "\r\n"
    flush $fd
}

proc cutis_readnl {fd len} {
    set buf [read $fd $len]
    # discard CR LF
    read $fd 2
    return $buf
}

proc cutis_bulk_read {fd} {
    set count [cutis_read_integer $fd]
    if {$count eq {nil}} return {}
    set len [expr {abs($count)}]
    set buf [cutis_readnl $fd $len]
    if {$count < 0} {return "***ERROR*** $buf"}
    return $buf
}

proc cutis_multi_bulk_read {fd} {
    set count [cutis_read_integer $fd]
    if {$count eq {nil}} return {}
    if {$count < 0} {
        set len [expr {abs($count)}]
        set buf [cutis_readnl $fd $len]
        return "***ERROR*** $buf"
    }
    set l {}
    for {set i 0} {$i < $count} {incr i} {
        lappend l [cutis_bulk_read $fd]
    }
    return $l
}

proc cutis_read_retcode {fd} {
    set retcode [string trim [gets $fd]]
    return $retcode
}

proc cutis_read_integer {fd} {
    string trim [gets $fd]
}

