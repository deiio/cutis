#!/usr/bin/tclsh
#
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com
#

set ::passed 0
set ::failed 0

cd [file dirname [info script]]

source "support/util.tcl"
source "support/cutis_api.tcl"

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

    test {DEL all keys to start with a clean DB} {
        foreach key [cutis_keys $fd *] {
            cutis_del $fd $key
        }
        cutis_dbsize $fd
    } {0}

    test {SET and GET an item} {
        cutis_set $fd x foobar
        cutis_get $fd x
    } {foobar}

    test {DEL against a single item} {
        cutis_del $fd x
        cutis_get $fd x
    } {}

    test {KEYS with pattern} {
        foreach key {key_x key_y key_z foo_a foo_b foo_c} {
            cutis_set $fd $key hello
        }
        lsort [cutis_keys $fd foo*]
    } {foo_a foo_b foo_c}

    test {KEYS to get all keys} {
        lsort [cutis_keys $fd *]
    } {foo_a foo_b foo_c key_x key_y key_z}

    test {DBSIZE} {
        cutis_dbsize $fd
    } {6}

    test {DEL all keys} {
        foreach key [cutis_keys $fd *] {
            cutis_del $fd $key
        }
        cutis_dbsize $fd
    } {0}

    test {Very big payload in GET/SET} {
        set buf [string repeat "abcd" 1000000]
        cutis_set $fd foo $buf
        cutis_get $fd foo
    } [string repeat "abcd" 1000000]

    test {SET 10000 numeric keys and access all them in reverse order} {
        for {set x 0} {$x < 10000} {incr x} {
            cutis_set $fd $x $x
        }
        set sum 0
        for {set x 9999} {$x >= 0} {incr x -1} {
            incr sum [cutis_get $fd $x]
        }
        format $sum
    } {49995000}

    test {DBSIZE should be 10001 now} {
        cutis_dbsize $fd
    } {10001}

    test {INCR against non existing key} {
        set res {}
        append res [cutis_incr $fd novar]
        append res [cutis_get $fd novar]
    } {11}

    test {INCR against key created by incr itself} {
        cutis_incr $fd novar
    } {2}

    test {INCR against key originally set with set} {
        cutis_set $fd novar 100
        cutis_incr $fd novar
    } {101}

    test {SETNX target key missing} {
        cutis_setnx $fd novar2 foobared
        cutis_get $fd novar2
    } {foobared}

    test {SETNX target key exists} {
        cutis_setnx $fd novar2 blabla
        cutis_get $fd novar2
    } {foobared}

    test {EXISTS} {
        set res {}
        cutis_set $fd newkey test
        append res [cutis_exists $fd newkey]
        cutis_del $fd newkey
        append res [cutis_exists $fd newkey]
    } {10}

    test {Zero length value in key. SET/GET/EXISTS} {
        cutis_set $fd emptykey {}
        set res [cutis_get $fd emptykey]
        append res [cutis_exists $fd emptykey]
        cutis_del $fd emptykey
        append res [cutis_exists $fd emptykey]
    } {10}

    test {Commands piplining} {
        puts -nonewline $fd "SET k1 4\r\nxyzk\r\nGET k1\r\nPING\r\n"
        flush $fd
        set res {}
        append res [string match +OK* [cutis_read_retcode $fd]]
        append res [cutis_bulk_read $fd]
        append res [string match +PONG* [cutis_read_retcode $fd]]
        format $res
    } {1xyzk1}

    test {Non existing command} {
        puts -nonewline $fd "foo\r\n"
        flush $fd
        string match -ERR* [cutis_read_retcode $fd]
    } {1}

    test {Basic LPUSH, RPUSH, LLENGTH, LINDEX} {
        cutis_lpush $fd mylist a
        cutis_lpush $fd mylist b
        cutis_rpush $fd mylist c
        set res [cutis_llen $fd mylist]
        append res [cutis_lindex $fd mylist 0]
        append res [cutis_lindex $fd mylist 1]
        append res [cutis_lindex $fd mylist 2]
    } {3bac}

    test {DEL a list} {
        cutis_del $fd mylist
        cutis_exists $fd mylist
    } {0}

    test {Create a long list and check every single element with LINDEX} {
        set ok 0
        for {set i 0} {$i < 1000} {incr i} {
            cutis_rpush $fd mylist $i
        }
        for {set i 0} {$i < 1000} {incr i} {
            if {[cutis_lindex $fd mylist $i] eq $i} {
                incr ok
            }
            if {[cutis_lindex $fd mylist [expr (-$i)-1]] eq [expr 999-$i]} {
                incr ok
            }
        }
        format $ok
    } {2000}

    test {Test elements with LINDEX in random access} {
        set ok 0
        for {set i 0} {$i < 1000} {incr i} {
            set r [expr int(rand() * 1000)]
            if {[cutis_lindex $fd mylist $r] eq $r} {
                incr ok
            }
            if {[cutis_lindex $fd mylist [expr (-$r)-1]] eq [expr 999-$r]} {
                incr ok
            }
        }
        format $ok
    } {2000}

    test {LLEN against non-list value error} {
        cutis_del $fd mylist
        cutis_set $fd mylist foobar
        cutis_llen $fd mylist
    } {-1}

    test {LINDEX against non-list value error} {
        cutis_lindex $fd mylist 0
    } {*ERROR*}

    test {LPUSH against non-list value error} {
        cutis_lpush $fd mylist 0
    } {-ERR*}

    test {RPUSH against non-list value error} {
        cutis_rpush $fd mylist 0
    } {-ERR*}

    test {RENAME basic usage} {
        cutis_set $fd mykey hello
        cutis_rename $fd mykey mykey1
        cutis_rename $fd mykey1 mykey2
        cutis_get $fd mykey2
    } {hello}

    test {RENAME source key should no longer exist} {
        cutis_exists $fd mykey
    } {0}

    test {RENAME against already existing key} {
        cutis_set $fd mykey a
        cutis_set $fd mykey2 b
        cutis_rename $fd mykey2 mykey
        set res [cutis_get $fd mykey]
        append res [cutis_exists $fd mykey2]
    } {b0}

    test {RENAMENX basic usage} {
        cutis_del $fd mykey
        cutis_del $fd mykey2
        cutis_set $fd mykey foobar
        cutis_renamenx $fd mykey mykey2
        set res [cutis_get $fd mykey2]
        append res [cutis_exists $fd mykey]
    } {foobar0}

    test {RENAMENX against already existing key} {
        cutis_set $fd mykey foo
        cutis_set $fd mykey2 bar
        cutis_renamenx $fd mykey mykey2
    } {-ERR*}

    test {RENAMENX against already existing key (2)} {
        set res [cutis_get $fd mykey]
        append res [cutis_get $fd mykey2]
    } {foobar}

    test {RENAME against non existing source key} {
        cutis_rename $fd nokey foobar
    } {-ERR*}

    test {RENAME where source and dest key is the same} {
        cutis_rename $fd mykey mykey
    } {-ERR*}

    test {DEL all keys again (DB 0)} {
        foreach key [cutis_keys $fd *] {
            cutis_del $fd $key
        }
        cutis_dbsize $fd
    } {0}

    test {DEL all keys again (CB 1)} {
        cutis_select $fd 1
        foreach key [cutis_keys $fd *] {
            cutis_del $fd $key
        }
        set res [cutis_dbsize $fd]
        cutis_select $fd 0
        format $res
    } {0}

    test {MOVE basic usage} {
        cutis_set $fd mykey foobar
        cutis_move $fd mykey 1
        set res {}
        lappend res [cutis_exists $fd mykey]
        lappend res [cutis_dbsize $fd]
        cutis_select $fd 1
        lappend res [cutis_get $fd mykey]
        lappend res [cutis_dbsize $fd]
        cutis_select $fd 0
        format $res
    } [list 0 0 foobar 1]

    test {MOVE against key existing in the target DB} {
        cutis_set $fd mykey hello
        cutis_move $fd mykey 1
    } {-ERR*}

    test {SET/GET keys in different DBs} {
        cutis_set $fd a hello
        cutis_set $fd b world
        cutis_select $fd 1
        cutis_set $fd a foo
        cutis_set $fd b bared
        cutis_select $fd 0
        set res {}
        lappend res [cutis_get $fd a]
        lappend res [cutis_get $fd b]
        cutis_select $fd 1
        lappend res [cutis_get $fd a]
        lappend res [cutis_get $fd b]
        cutis_select $fd 0
        format $res
    } {hello world foo bared}

    test {Basic LPOP/RPOP} {
        cutis_del $fd mylist
        cutis_rpush $fd mylist 1
        cutis_rpush $fd mylist 2
        cutis_lpush $fd mylist 0
        set res {}
        lappend res [cutis_lpop $fd mylist]
        lappend res [cutis_rpop $fd mylist]
        lappend res [cutis_lpop $fd mylist]
        lappend res [cutis_llen $fd mylist]
    } {0 2 1 0}

    test {LPOP/RPOP against empty list} {
        cutis_lpop $fd mylist
    } {}

    test {LPOP against non list value} {
        cutis_set $fd notalist foo
        cutis_lpop $fd notalist
    } {*ERROR*POP against*}

    test {Mass LPUSH/LPOP} {
        set num 0
        for {set i 0} {$i < 1000} {incr i} {
            cutis_lpush $fd mylist $i
            incr num $i
        }
        set num2 0
        for {set i 0} {$i < 500} {incr i} {
            incr num2 [cutis_lpop $fd mylist]
            incr num2 [cutis_rpop $fd mylist]
        }
        expr $num == $num2
    } {1}

    test {LRANGE basic usage} {
        for {set i 0} {$i < 10} {incr i} {
            cutis_rpush $fd mylist $i
        }
        list [cutis_lrange $fd mylist 1 -2] \
             [cutis_lrange $fd mylist -3 -1] \
             [cutis_lrange $fd mylist 4 4]
    } {{1 2 3 4 5 6 7 8} {7 8 9} 4}

    test {LRANGE inverted indexes} {
        cutis_lrange $fd mylist 6 2
    } {}

    test {LRANGE out of range indexes including the full list} {
        cutis_lrange $fd mylist -1000 1000
    } {0 1 2 3 4 5 6 7 8 9}

    test {LRANGE against non existing key} {
        cutis_lrange $fd nosuchkey 0 1
    } {}

    test {LTRIM basic usage} {
        cutis_del $fd mylist
        for {set i 0} {$i < 100} {incr i} {
            cutis_lpush $fd mylist $i
            cutis_ltrim $fd mylist 0 4
        }
        cutis_lrange $fd mylist 0 -1
    } {99 98 97 96 95}

    # Leave the user with a clean DB before to exit
    test {DEL all keys again (DB 0)} {
        foreach key [cutis_keys $fd *] {
            cutis_del $fd $key
        }
        cutis_dbsize $fd
    } {0}

    test {DEL all keys again (DB 1)} {
        cutis_select $fd 1
        foreach key [cutis_keys $fd *] {
            cutis_del $fd $key
        }
        set res [cutis_dbsize $fd]
        cutis_select $fd 0
        format $res
    } {0}

    puts ""
    puts "[expr $::passed+$::failed] tests, $::passed passed, $::failed failed"
    if {$::failed > 0} {
        puts [colorstr bold-red "\n*** WARNING!!! $::failed FAILED TESTS ***\n"]
    }

    close $fd
}

if {$argc == 0} {
    main 127.0.0.1 6380
} else {
    main [lindex $argv 0] [lindex $argv 1]
}
