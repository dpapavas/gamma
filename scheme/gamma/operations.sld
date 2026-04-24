;; -*- mode:scheme; coding: utf-8 -*-

(define-library (gamma operations)
  (export offset extrusion hull minkowski-sum union difference intersection
          complement boundary interior clip deflate color-selection color-vertices
          color-faces subdivide-catmull-clark subdivide-doo-sabin subdivide-loop
          subdivide-sqrt-3 remesh perturb refine fair smooth-shape
          deform corefine complement components chamfer chamfer-inner
          chamfer-outer chamfer-both make-chamfer make-inner-chamfer
          make-outer-chamfer fillet fillet-inner fillet-outer fillet-both
          make-fillet make-inner-fillet make-outer-fillet

          linear-extrusion angular-extrusion)

  (import (gamma %operations) (gamma base) (gamma transformation)
          (scheme base) (scheme case-lambda) (scheme inexact))

  (begin
    (define (chamfer shape . rest)
      (apply chamfer-outer (apply chamfer-inner shape rest) rest))

    (define (fillet shape . rest)
      (apply fillet-outer (apply fillet-inner shape rest) rest))

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

       ((base r a) (angular-extrusion base r (/ a -2) (/ a 2)))))))
