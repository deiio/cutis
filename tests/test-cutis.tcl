#!/usr/bin/tclsh

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

if {$argc == 0} {
    main 127.0.0.1 6380
} else {
    main [lindex $argv 0] [lindex $argv 1]
}