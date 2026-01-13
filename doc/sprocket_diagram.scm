(import (gamma inexact) (gamma transformation)
        (gamma polygons) (gamma operations))

(set-curve-tolerance! 1/50)

(define d_1 5)
(define p 8)
(define z 9)
(define b_f1 14/5)

(define d (/ p (sin-degrees (/ 180 z))))
(define r_a (/ (+ (+ d (* p (- 1 (/ 8/5 z))) (- d_1))
                  (+ d (* 5/4 p) (- d_1))) 4))

(define r_e (* 12/100 d_1 (+ z 2)))
(define r_i (* 505/1000 d_1))
(define alpha (- 140 (/ 90 z)))

(define teeth (iota 4 -3/2))

(define (place-tooth part r c n)
  (transform
   part
   (translation 0 (- r))
   (rotation (* c alpha))
   (translation 0 (/ d 2))
   (rotation (* -1 n (/ 360 z)))))

(define roller-circles
  (list-for ((s teeth))
    (place-tooth (circle r_i) 0 0 s)))

(define flank-circles
  (list-for ((s teeth)
             (t (list -1/2 1/2)))
    (place-tooth (circle r_e) (+ r_i r_e) t s)))

(apply
 output
 1

 (color-vertices
  (apply
   union
   (apply
    difference

    (apply
     hull
     (point (- r_a) 0)
     (point r_a 0)
     (list-for ((s teeth)
                (t (list -1/2 1/2)))
       (place-tooth (point 0 0) r_i t s)))

    roller-circles)

   (map
    (partial apply intersection (circle r_a))
    (tile (roll flank-circles 1) 2 2))) 1/3 1/3 1/3 1)

 (map
  (lambda (x)
    (color-vertices x 0 0 0 1/4))
    (cons*
     (circle r_a)
     (apply hull (list-for ((s teeth)
                            (t (list -1/2 1/2)))
                   (place-tooth (point 0 0) r_i t s)))
     (append flank-circles roller-circles))))
