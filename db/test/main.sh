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

test_run_invalid_1() { run -c "run bogus" | error 'invalid mode specified'; }
test_run_invalid_2() { run -c "run all bogus" | syntax_error; }
test_run_invalid_3() { run -c "run single bogus" | syntax_error; }

test_run_no_window() {
    (run --execute=- -c "run" | match hello world) <<EOF
set program echo
set args hello world
EOF
}

test_run() {
    i=$(cat <<EOF
set program echo
set args hello world
window test
target one
split
focus 2
target two
EOF
     )

    echo -n "$i" | run --execute=- -c "run all" |
        match -o one:one -o two:two hello world || return 1
    echo -n "$i" | run --execute=- -c "run single" |
        match -o two:two hello world || return 1
}

test_set() {
    while read -ra v; do
        run -c "set ${v[*]}" -c "show ${v[0]}" | match "${v[*]:1}" || return 1
    done <<EOF
    program hello world
    args --hello --world
    default-color 0.1 0.2 0.3 0.4
    mouse-sensitivity 0.123
EOF
}
