run() { rungamma "$@"; }

test_invalid() { run -c "bogus somearg" | error 'invalid command "bogus"'; }

test_quit_invalid() { run -c "quit bogus" | syntax_error; }
test_quit() { run -c "quit" | ok; }

test_exit_invalid() { run -c "exit bogus" | syntax_error; }
test_exit() { run -c "exit" | ok; }

test_info_unknown() { run -c "window test" -c "info nothing" | error "no such subject"; }
test_info_invalid_1() { run -c "window test" -c "info viewports bogus" | syntax_error; }
test_info_invalid_2() { run -c "window test" -c "info windows bogus" | syntax_error; }
test_info_invalid_3() { run -c "window test" -c "info objects bogus" | syntax_error; }
test_info_windows() { run -c "info windows" | match "No existing windows."; }
test_info_bindings() { run -c "info bindings" | match "No existing bindings."; }

test_run_invalid_1() { run -c "run bogus" | error 'invalid mode specified'; }
test_run_invalid_2() { run -c "run all bogus" | syntax_error; }
test_run_invalid_3() { run -c "run single bogus" | syntax_error; }

match_run() {
    match --debugger-address=gammadb-$(while ! pidof -s gammadb; do :; done) "$@"
}

test_run_no_window() {
    (run --execute=- | match_run hello world) <<EOF
set program echo
set args hello world
run
EOF
}

test_run() {
    i=$(cat <<EOF
set program echo
set args hello world
define flag
define name value
window test
target one
split
focus 2
target two
EOF
     )

    echo -n "$i" | run --execute=- -c "run all" |
        match_run -Dflag -Dname=value -o one:one -o two:two hello world ||
        return 1

    echo -n "$i" | run --execute=- -c "run single" |
        match_run -Dflag -Dname=value -o two:two hello world ||
        return 1
}

test_output() {
    (run --execute=- -c "output test.stl" |
        match -Dname=value -o test.stl:one hello world) <<EOF
set program echo
set args hello world
define name value
window test
target one
split
output test.stl
EOF
}

test_print_missing() { run -c "window test" -c "print" | error "no output file name specified"; }
test_print_invalid() { run -c "window test" -c "print file.ps bogus" | syntax_error; }
test_print_unknown() { run -c "window test" -c "print file.ext" | error "output file has unknown extension"; }
test_print() {
    f="$(mktemp -u)"
    trap "rm $f.*" RETURN

    for x in ".ps" ".eps" ".pdf" ".svg"; do
        run -c "window test" -c "load <$(dirname $0)/convex.off" -c "print $f$x" | ok \
            && test -f "$f$x" || return 1
    done
}

test_set() {
    while read -ra v; do
        run -c "set ${v[*]}" -c "show ${v[0]}" | match "${v[*]:1}" || return 1
    done <<EOF
    program hello world
    args --hello --world
    present-on-reload no
    resize-on-split yes
    default-vertex-color 0.1 0.2 0.3 0.4
    edge-line-width 2.3
    vertex-point-size 4.5
    mouse-sensitivity 0.123
    default-view 100
    default-zoom 0.5
    default-rotation 10 20 30
    default-translation 50 60 70
EOF
}

# This is a special case.  Once we set it to "yes", we expect no
# further ouput in the show command.

test_set_quiet() {
    run -c "set quiet yes" -c "show quiet" | ok
}

test_bind_no_key() { run -c "bind" | error "no key specified"; }
test_bind_no_command() { run -c "bind a" | error "no command specified"; }
test_bind_invalid_key_1() { run -c "bind X-a" | error "invalid modifier 'X' specified"; }
test_bind_invalid_key_2() { run -c "bind bogus" | error "invalid key 'bogus' specified"; }
test_bind_invalid_key_3() { run -c "bind C-bogus" | error "invalid key 'bogus' specified"; }
test_bind_invalid_key_4() { run -c "bind "$'\a' | error "invalid key specified"; }
test_bind_invalid_key_5() { run -c "bind M-"$'\a' | error "invalid key specified"; }

test_unbind_no_key() { run -c "unbind" | error "no key specified"; }
test_unbind_invalid_1() { run -c "unbind a" | error "no such binding"; }
test_unbind_invalid_2() { run -c "bind a foo" -c "unbind a bogus" | syntax_error; }

test_define_no_name() { run -c "define" | error "no parameter specified"; }
test_define() {
    run -c "define flag" -c "define name value" -c "info definitions" | (
        match "Name" "Value"
        match "flag" ""
        match "name" "value"
        ok
    )
}

test_bind() {
    run -c "bind a initial" \
        -c "bind B uppercase" \
        -c "bind escape to unbind" \
        -c "bind f1 to reuse" \
        -c "bind C-f foo" \
        -c "bind C-F Foo" \
        -c "bind C-M-b bar" \
        -c "bind S-tab hello" \
        -c "bind S-s-enter world" \
        -c "bind a rebound" \
        -c "unbind escape" \
        -c "unbind f1" \
        -c "bind f1 reused" \
        -c "info bindings" | (
        match "Key" "Command"
        match "a" "rebound"
        match "B" "uppercase"
        match "f1" "reused"
        match "C-f" "foo"
        match "C-F" "Foo"
        match "C-M-b" "bar"
        match "S-tab" "hello"
        match "S-s-enter" "world"
        ok
    )
}
