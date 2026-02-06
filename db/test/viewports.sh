run() { rungamma -c "window test" \
                 -c "resize 1001 1001" \
                 -c "target test" \
                 "$@" \
                 -c "info viewports"; }

viewports() {
    match \# Orig. Size Trans. Rotation Zoom Pr. Name ""

    while [ $# -gt 0 ]; do
        match "${@:1:9}"
        shift 9
    done

    match ""
}

test_target_invalid() { run -c "target name bogus" | syntax_error; }
test_target_0() {
    run -c "target" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 "" "*"
}
test_target_1() {
    run -c "target other" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 other "*"
}

test_focus_missing() { run -c "focus" | error "no viewport index specified"; }
test_focus_invalid() { run -c "focus 1 bogus" | syntax_error; }
test_focus() {
    run -c "split" -c "focus 2" -c "target other" \
        | viewports 1 "0, 0" "500, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "" \
                    2 "500, 0" "500, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 other "*"
}

test_rotate_invalid() { run -c "rotate bogus" | syntax_error; }
test_rotate_invalid_1() { run -c "rotate 10 bogus" | syntax_error; }
test_rotate_invalid_2() { run -c "rotate 10 20 bogus" | syntax_error; }
test_rotate_invalid_3() { run -c "rotate 10 20 30 bogus" | syntax_error; }
test_rotate_0() {
    run -c "rotate 5 10 15" -c "rotate" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*"
}

test_rotate_1() {
    run -c "rotate 30" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "30, 0, 0" 0.7 50 test "*"
}

test_rotate_2() {
    run -c "rotate 30 40" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "30, 40, 0" 0.7 50 test "*"
}

test_rotate_3() {
    run -c "rotate 30 40 50" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "30, 40, 50" 0.7 50 test "*"
}

test_translate_invalid() { run -c "translate bogus" | syntax_error; }
test_translate_invalid_1() { run -c "translate 1 bogus" | syntax_error; }
test_translate_invalid_2() { run -c "translate 1 2 bogus" | syntax_error; }
test_translate_invalid_3() { run -c "translate 1 2 3 bogus" | syntax_error; }
test_translate_0() {
    run -c "translate 1 2 3" -c "translate" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*"
}

test_translate_1() {
    run -c "translate 1.2" \
        | viewports 1 "0, 0" "1000, 1000" "1.2, 0, 0" "0, 0, 0" 0.7 50 test "*"
}

test_translate_2() {
    run -c "translate 1.2 3.4" \
        | viewports 1 "0, 0" "1000, 1000" "1.2, 3.4, 0" "0, 0, 0" 0.7 50 test "*"
}

test_translate_3() {
    run -c "translate 1.2 3.4 5.6" \
        | viewports 1 "0, 0" "1000, 1000" "1.2, 3.4, 5.6" "0, 0, 0" 0.7 50 test "*"
}

test_pan_invalid() { run -c "pan bogus" | syntax_error; }
test_pan_invalid_1() { run -c "pan 1 bogus" | syntax_error; }
test_pan_invalid_2() { run -c "pan 1 2 bogus" | syntax_error; }
test_pan_0() {
    run -c "pan 1 2" -c "pan" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*"
}

test_pan_1() {
    run -c "rotate 90 0 90" -c "pan 1" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 1" "90, 0, 90" 0.7 50 test "*"
}

test_pan_2() {
    run -c "rotate 90 0 90" -c "pan 1 2" \
        | viewports 1 "0, 0" "1000, 1000" "2, 0, 1" "90, 0, 90" 0.7 50 test "*"
}

test_zoom_invalid() { run -c "zoom bogus" | syntax_error; }
test_zoom_invalid_1() { run -c "zoom 1 bogus" | syntax_error; }
test_zoom_0() {
    run -c "zoom 1" -c "zoom" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*"
}

test_zoom_1() {
    run -c "zoom 0.5" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 1.2 50 test "*"
}

test_view_missing() { run -c "view" | error "no projection specified"; }
test_view_invalid() { run -c "view bogus" | error "invalid projection specified"; }
test_view_invalid_1() { run -c "view orthographic bogus" | syntax_error; }
test_view_invalid_2() { run -c "view 70 bogus" | syntax_error; }
test_view_1() {
    run -c "view orthographic" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 "Or." test "*"
}

test_view_2() {
    run -c "view orthographic" -c "view perspective" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*"
}

test_view_3() {
    run -c "view 70" \
        | viewports 1 "0, 0" "1000, 1000" "0, 0, 0" "0, 0, 0" 0.7 70 test "*"
}

test_split_invalid() { run -c "split bogus" | error "invalid split direction specified"; }
test_split_invalid_1() { run -c "split horizontally bogus" | syntax_error; }
test_split_invalid_2() { run -c "split horizontally 2 bogus" | syntax_error; }
test_split_invalid_3() { run -c "split vertically 2 1 bogus" | syntax_error; }
test_split_1() {
    run -c "split" \
        | viewports 1 "0, 0" "500, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*" \
                    2 "500, 0" "500, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 2 ""
}

test_split_2() {
    run -c "split vertically 2" \
        | viewports 1 "0, 0" "1000, 500" "0, 0, 0" "0, 0, 0" 0.7 50 test "*" \
                    2 "0, 500" "1000, 500" "0, 0, 0" "0, 0, 0" 0.7 50 2 ""
}

test_split_3() {
    run -c "split horizontally 4" \
        | viewports 1 "0, 0" "250, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*" \
                    2 "250, 0" "250, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 2 "" \
                    3 "500, 0" "250, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 3 "" \
                    4 "750, 0" "250, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 4 ""
}

test_split_4() {
    run -c "split vertically 4" \
        | viewports 1 "0, 0" "1000, 250" "0, 0, 0" "0, 0, 0" 0.7 50 test "*" \
                    2 "0, 250" "1000, 250" "0, 0, 0" "0, 0, 0" 0.7 50 2 "" \
                    3 "0, 500" "1000, 250" "0, 0, 0" "0, 0, 0" 0.7 50 3 "" \
                    4 "0, 750" "1000, 250" "0, 0, 0" "0, 0, 0" 0.7 50 4 ""
}

test_split_5() {
    run -c "split horizontally 4 2" \
        | viewports 1 "0, 0" "250, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 test "*" \
                    2 "250, 0" "250, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 2 "" \
                    3 "500, 0" "500, 1000" "0, 0, 0" "0, 0, 0" 0.7 50 3 ""
}

test_target_option() {
    run --target "other" -c "window other" |
        viewports 1 "0, 0" "499, 499" "0, 0, 0" "0, 0, 0" 0.7 50 other "*"
}
