(import (gamma operations)
        (gamma base)
        (only (gamma operations) remesh)
        (only (gamma base) output)
        (rename (gamma operations) (remesh %remesh))
        (rename (gamma base) (output %output)))

(define output (lambda (name . rest) (car rest)))
(load "../doc/fitting.scm")


(%output 4 (make-fitting
            2 3 1
            (circle 5/2) 0
            (circle 4) 180))

(%output 1 (make-fitting
            2 3 1
            (translate (circle 5/2) 3/2 0) 0
            (circle 4) 180))

(%output 2 (make-fitting
            5 3 1
            (circle 4) 0
            (circle 5/2) 90
            (circle 4) 180))

(define remesh (lambda (a b c d) (%remesh a c d)))
(load "../doc/fitting.scm")

(%output 3 (make-fitting
           5 3 1
           (circle 4) -45
           (circle 4) 45
           (circle 4) 180))
