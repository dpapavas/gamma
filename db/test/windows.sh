run() { rungamma -c "window test" -c "resize 1000 1000" "$@" -c "info windows"; }

windows() {
    match \# Size Vis. Name

    while [ $# -gt 0 ]; do
        match "${@:1:4}"
        shift 4
    done

    match ""
}

test_hide_invalid() { run -c "hide bogus" | syntax_error; }
test_hide() { run -c "present" -c "hide" | windows 1 "1000, 1000" No test; }

test_present_invalid() { run -c "present bogus" | syntax_error; }
test_present() { run -c "present" | windows 1 "1000, 1000" Yes test; }

test_resize_missing_size() { run -c "resize" | error "new size not specified"; }
test_resize_missing_height() { run -c "resize 100" | error "new size not specified"; }
test_resize_invalid() { run -c "resize 100 100 bogus" | syntax_error; }
test_resize() { run -c "resize 100 100" | windows 1 "100, 100" No test; }

test_window_invalid() { run -c "window name bogus" | syntax_error; }
test_window_no_name() { run -c "window" | error "no window name specified"; }
test_window() {
    run -c "window name" | windows 1 "500, 500" No name \
                                   2 "1000, 1000" No test
}
