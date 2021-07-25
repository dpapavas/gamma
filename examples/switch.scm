;; -*- mode:scheme; coding: utf-8 -*-

;; This program produces the actuator part of a switch assembly
;; (modeled as a replacement form an OEM part in the left-hand switch
;; housing of a BMW F650GS motorcycle).  The geometry is produced
;; using essentially only primitives and boolean operations (plus a
;; few convex hulls), demonstrating that complex geometry can result
;; from the combination of simple shapes.

(import (srfi 1)
        (gamma inexact) (gamma transformation) (gamma polygons)
        (gamma polyhedra) (gamma operations))

;; Some basic parameters of the switch knob; length and width.

(define L 28)
(define W 12)

(set-curve-tolerance! 1/500)

;; The basic shape from which the body will be carved.  It is made up of the
;; convex hull of four circles (for the rounded corners) and the intersection of
;; a rectangle and ellipse for the curved sides.

(define-output
  basic-body
  (~> (append
       (list-for ((s (list -3 3)))
         (translate (circle 1) (+ (/ L -2) 1) s))

       (list (intersection
              (rectangle L W)
              (ellipse (* 11/20 L) (/ W 2))))

       (list-for ((s (list -5/2 5/2)))
         (translate (circle 2) (- (/ L 2) 2) s)))

      (apply hull _)
      (linear-extrusion _ 0 20)))

;; We proceed in a series of steps, to further shape the simple extrusion
;; created above and to add other structures of the switch.

(define-output
  shaped-body
  (~> basic-body

      (intersection _
                    ;; We angle and bevel the top part of the switch by
                    ;; intersecting the extruded shape with a hull, formed by
                    ;; the rotated top portion of a pair of spheres, plus a
                    ;; rectangular base (formed by a zero-length extrusion).

                    (hull
                     (linear-extrusion (rectangle L W) 0)
                     (apply
                      union
                      (list-for ((s (list 0 1)))
                        (let* ((h 3)
                               (c W)
                               (R (+ (/ (* c c) 8 h) (/ h 2))))
                          (transform
                           (clip
                            (transformation-apply
                             (translation 0 0 (- h R)) (sphere R))
                            (plane 0 0 -1 0))
                           (translation
                            (+ (/ c 2) (* s (- (/ L (cos° 10)) c))) 0 0)
                           (rotation -10 1)
                           (translation (/ L -2) 0 6)))))))

      ;; Add some structures.

      (union _
             (hull
              ;; The front pivot arm

              (~> (rectangle 2 8)
                  (extrusion _
                             (translation 0 0 0)
                             (translation -1 0 13))
                  (scaling-λ 1 1 -1)
                  (translation-λ (- 4 1) 0 13/2))

              ;; The contact housing

              (~> (cylinder 4 10)
                  flush-bottom
                  (rotation-λ 27 1)
                  (translation-λ (- 4 7) 0 -5))))

      ;; The contact.  This should not be included in a production build of
      ;; course, as the contact needs to be metal, but it can be useful for test
      ;; parts, meant to validate proper fit.

      (union _
             (~> (apply
                  hull
                  (list-for ((z (list 0 -2)))
                    (~> (sphere 7/4)
                        (scaling-λ 1 1 4/7)
                        (translation-λ 0 0 z))))
                 (rotation-λ 27 1)
                 (translation-λ (- 4 7) 0 -5)))

      (difference _
                  ;; We cut out a cylindrical portion from the top, to form a
                  ;; more comfortable surface for the thumb.

                  (let ((r 100))
                    (~> (cylinder r 100)
                        (with-curve-tolerance 1/5000 _)
                        (rotation-λ 90 0)
                        (translation-λ -9 0 (+ r 10))))

                  ;; We also cut out the front lower part of the switch.  We
                  ;; want rounded corners, so we form the cutout shape as the
                  ;; hull of cylindrical "edges".

                  (let* ((r 1/2)        ; The corner radius
                         (x_0 (+ r 4)))
                    (apply hull
                           (list-for ((δ (list (list (- x_0 1) 0 -8)
                                               (list x_0 0 6)
                                               (list (+ x_0 10) 0 9)
                                               (list (+ x_0 10) 0 0))))
                             (~> (cylinder 1/2 100)
                                 (transform _
                                            (rotation 90 0)
                                            (apply translation δ)))))))))

(define-output
  switch
  (union

   ;; The front pivot

   (difference
    (union
     shaped-body

     (~> (union
          (cylinder 5/2 8)
          (flush-bottom (cylinder 1 (+ 4 17/10))))
         (difference _ (hull (flush-top (cylinder (+ 3/2 1/10) 4))
                             (point 0 0 1)))
         (rotation-λ 90 0)
         (translation-λ (- 4 3/2) 0 -17/2))))

   ;; The rear pivot: a hull of three cylinder, one of which, is intersected
   ;; with the body, for a neater joint.

   (let ((r 3)
         (l 17/2)
         (C (lambda (ρ h x z . rest)
              (~> (cylinder ρ h)
                  (rotation-λ 90 0)
                  (translation-λ (+ (/ L -2) 3 x) 0 z)))))
     (difference
      (hull (intersection (C r 100 0 0) shaped-body)
            (C r l 2 0) (C r l 0 -2))
      (C (+ 5/4 1/10) 100 0 -2)))))
