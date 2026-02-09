(import (gamma inexact) (gamma transformation)
        (gamma polygons) (gamma operations))

(define-option draft?)
(set-curve-tolerance! (if draft? 1/20 1/1000))

(define-parameter d_1 5)        ; Maximum roller diameter (mm)
(define-parameter p 8)          ; pitch (mm)
(define-parameter z 17)         ; No of sprocket teeth
(define-parameter b_f1 14/5)    ; Sprocket width

;; Dimensions calculated from the preceding parameters.

(define d
  (/ p (sin-degrees (/ 180 z))))      ; Pitch-circle diameter
(define d_a
  (/ (+ (+ d (* p (- 1 (/ 8/5 z))) (- d_1)) ; Tip diameter
        (+ d (* 5/4 p) (- d_1))) 2))
(define r_e (* 12/100 d_1 (+ z 2)))   ; Tooth flank radius
(define r_i (* 505/1000 d_1))         ; Roller seating radius
(define alpha (- 140 (/ 90 z)))       ; Roller seating angle

(define (place-tooth part r c n)
  (transform part
             (translation 0 (- r))
             (rotation (* c alpha))
             (translation 0 (/ d 2))
             (rotation (* -1 n (/ 360 z)))))

;; The central plate with roller seats cut out.

(define seats
  (apply
   difference
   (apply hull (list-for ((s (iota z)) (t (list -1/2 1/2)))
                 (place-tooth (point 0 0) r_i t s)))
   (list-for ((s (iota z)))
       (place-tooth (circle r_i) 0 0 s))))

;; Tooth flanks, extending above the roller seats.

(define flanks
  (list-for ((s (iota z)))
    (let ((place
           (partial place-tooth (circle r_e) (+ r_i r_e))))
      #>3 (intersection
       (circle (/ d_a 2))
       (place 1/2 s)
       (place -1/2 (+ s 1))))))

(output 1 seats)
(define-output sprocket
  (linear-extrusion (apply union seats flanks) b_f1))
