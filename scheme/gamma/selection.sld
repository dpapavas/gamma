(define-library (gamma selection)
  (include-shared "selection")

  (cond-expand
   ((not core-gamma)
    (import (scheme base) (gamma operations))

    (begin
      (define (vertices-not-in volume) (vertices-in (complement volume)))
      (define (faces-not-in volume) (faces-partially-in (complement volume)))
      (define (faces-outside volume) (faces-in (complement volume)))
      (define (edges-not-in volume) (edges-partially-in (complement volume)))
      (define (edges-outside volume) (edges-in (complement volume))))

    (export vertices-not-in faces-not-in faces-outside edges-not-in edges-outside)))

  (export vertices-in faces-in faces-partially-in edges-in edges-partially-in
          expand-selection contract-selection))
