;; -*- mode:scheme; coding: utf-8 -*-

(define-library (gamma operations)
  (include-shared "operations")

  (export offset extrusion hull minkowski-sum union difference intersection
          complement boundary clip deflate color-selection
          subdivide-catmull-clark subdivide-doo-sabin subdivide-loop
          subdivide-sqrt-3 remesh perturb refine fair smooth-shape
          deform corefine)

  (cond-expand
   ((not core-gamma)
    (import (scheme base) (scheme case-lambda) (scheme inexact)
            (gamma base) (gamma transformation))

    (begin
      (define linear-extrusion
        (case-lambda
         ((base h) (linear-extrusion base (/ h -2) (/ h 2)))
         ((base a b) (if (= a b)
                         (extrusion base (translation 0 0 a))
                         (extrusion base
                                    (translation 0 0 a)
                                    (translation 0 0 b))))

         ((base . rest) (apply extrusion base
                               (list-for ((z rest)) (translation 0 0 z))))))

      (define angular-extrusion
        (case-lambda
         ((base r a b) (if (= a b)
                           (extrusion base (transformation-append
                                            (rotation a 1)
                                            (translation r 0 0)))
                           (apply extrusion base
                                  (let* ((ε (set-curve-tolerance! '()))
                                         (n (exact
                                             (ceiling
                                              (/ (abs (- b a))
                                                 (* 360 (/ (acos (- 1 (/ ε (abs r))))
                                                           (acos -1))))))))
                                    (list-for ((s (linear-partition a b n)))
                                      (transformation-append
                                       (rotation (+ a s) 1)
                                       (translation r 0 0)))))))

         ((base r a) (angular-extrusion base r (/ a -2) (/ a 2))))))

    (export linear-extrusion angular-extrusion))))
