# Test if TERM looks like to support colors
proc color_term {} {
    expr {[info exists ::env(TERM)] && [string match *xterm* $::env(TERM)]}
}

proc colorstr {color str} {
    if {[color_term]} {
        set b 0
        if {[string range $color 0 4] eq {bold-}} {
            set b 1
            set color [string range $color 5 end]
        }

        switch $color {
            red     {set colorcode {31}}
            green   {set colorcode {32}}
            yellow  {set colorcode {33}}
            blue    {set colorcode {34}}
            magenta {set colorcode {35}}
            cyan    {set colorcode {36}}
            white   {set colorcode {37}}
            default {set colorcode {37}}
        }

        if {$colorcode ne {}} {
            return "\033\[$b;${colorcode};49m$str\033\[0m"
        }
    } else {
        return $str
    }
}