(import (srfi 1)
        (gamma transformation) (gamma polygons)
        (gamma operations) (gamma volumes) (gamma selection))

(define-option draft?)
(set-curve-tolerance! (if draft? 1/50 1/100))
(define remesh-target (if draft? 1 1/2))
(define remesh-iterations (if draft? 1 2))
(define fairing-continuity 1)

(define (make-fitting radius height width . shapes)
  (define (place f)      ; Distribute ends or planes
    (apply map (partial* ; radially, at the given angles.
                transform (f _) (transformation-append
                                 (rotation _ 1)
                                 (translation 0 0 radius)))
           (apply zip (tile shapes 2))))

  (define unfaired
    (apply      ; Take the difference of two version of the
     difference ; mesh to form the hollow center.
     (list-for ((delta (list 0 (- width))))
       (let ((place-extruded-to
              (lambda (h)   ; Place end shapes, extruded
                (place      ; to height h.
                 (partial*
                  linear-extrusion (offset _ delta) 0 h)))))
         (apply         ; Hull unextruded end shapes to form
          union         ; the body and add the extruded ends.
          (apply hull (place-extruded-to 0))
          (place-extruded-to height))))))

  (fair
   #>3 (remesh    ; Remesh, keeping the sharp edges of the
    #>1 unfaired  ; ends intact.
    (edges-by-sharpness-angle 90)
    remesh-target remesh-iterations)
   (vertices-in   ; Fair the central part only, keeping the
    (apply        ; ends unaltered.
     intersection
     (place (const (bounding-halfspace 0 0 1 0)))))
   fairing-continuity))

(define-output fitting (make-fitting
                        5 3 1
                        (circle 4) -45
                        (circle 4) 45
                        (circle 4) 180))
