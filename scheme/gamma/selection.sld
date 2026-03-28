(define-library (gamma selection)
  (export vertices-in faces-in faces-partially-in edges-in edges-partially-in
          expand-selection contract-selection edges-by-sharpness
          faces-by-sharpness faces-through-segment edges-through-segment
          faces-through-ray edges-through-ray faces-through-line
          edges-through-line faces-through-plane edges-through-plane

          vertices-not-in faces-not-in faces-outside edges-not-in edges-outside)

  (import (gamma %selection) (only (gamma %operations) complement) (scheme base))

  (begin
    (define (vertices-not-in volume) (vertices-in (complement volume)))
    (define (faces-not-in volume) (faces-partially-in (complement volume)))
    (define (faces-outside volume) (faces-in (complement volume)))
    (define (edges-not-in volume) (edges-partially-in (complement volume)))
    (define (edges-outside volume) (edges-in (complement volume)))))
