#!/usr/bin/tclsh
#
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com
#

set ::passed 0
set ::failed 0

source "tests/support/util.tcl"

proc test {name code expected} {
    set retval [uplevel 1 $code]
    if {$expected eq $retval || [string match $expected $retval]} {
        puts "\[[colorstr green PASSED]\] $name"
        incr ::passed
    } else {
        puts "\[[colorstr red FAILED]\] $name"
        puts "!! ERROR expected\n'$expected'\nbut got\n'$retval'"
        incr ::failed
    }
}

proc main {server port} {
    set fd [cutis_connect $server $port]

    test {SET and GET an item} {
        cutis_set $fd x foobar
        cutis_get $fd x
    } {foobar}

    test {DEL against a single item} {
        cutis_del $fd x
        cutis_get $fd x
    } {}

    test {EXISTS} {
        set res {}
        cutis_set $fd newkey test
        append res [cutis_exists $fd newkey]
        cutis_del $fd newkey
        append res [cutis_exists $fd newkey]
    } {10}

    puts ""
    puts "[expr $::passed+$::failed] tests, $::passed passed, $::failed failed"
    if {$::failed > 0} {
        puts [colorstr bold-red "\n*** WARNING!!! $::failed FAILED TESTS ***\n"]
    }

    close $fd
}

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
        lappend l [cutio_bulk_read $fd]
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

### Actual API ###
proc cutis_set {fd key val} {
    cutis_writenl $fd "set $key [string length $val]\r\n$val"
    cutis_read_retcode $fd
}

proc cutis_get {fd key} {
    cutis_writenl $fd "get $key"
    cutis_bulk_read $fd
}

proc cutis_del {fd key} {
    cutis_writenl $fd "del $key"
    cutis_read_retcode $fd
}

proc cutis_exists {fd key} {
    cutis_writenl $fd "exists $key"
    cutis_read_integer $fd
}

if {$argc == 0} {
    main 127.0.0.1 6380
} else {
    main [lindex $argv 0] [lindex $argv 1]
}
