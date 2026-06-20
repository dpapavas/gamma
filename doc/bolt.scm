(import (gamma transformation) (gamma polygons)
        (gamma polyhedra) (gamma operations))

(define-option draft?)

(define (thread d P L)
  (let* ((H (* 1/2 (sqrt 3) P))       ; Height
         (d_1 (- d (* 5/4 H)))        ; Minor diameter
         (n (if draft? 20 50)))       ; No. of steps

    (union
     (flush-bottom (prism n (/ d_1 2) L)) ; The shaft
     #>3
     (apply
      extrusion
      (let ((a (/ P 16))              ; Coordinates of
            (b (* P 3/8))             ; basic profile
            (c (/ H -4))              ; vertices
            (d (* H 5/8)))
        #>1
        (simple-polygon               ; The basic profile
         (point b c) (point b 0) (point a d)
         (point (- a) d) (point (- b) 0) (point (- b) c)))

      ;; The transformations making up the helical path

      (list-for ((s (iota (ceiling (/ L P))))
                 (t (iota n 0 (/ 1 n))))
        (transformation-append
         (rotation 90 1)
         (rotation (* 360 t) 0)
         (translation (- (* P (+ s t)) L) (/ d_1 2) 0)
         (if (zero? s) (scaling t t 1) (scaling 1 1 1))))))))

(define-output bolt
  (union
   (minkowski-sum                  ; The hexagonal head
    (flush-top (prism 6 7 5)) (octahedron 1 1 1/2))
   (thread 8 5/4 20)))
