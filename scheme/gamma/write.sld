(define-library (gamma write)
  (export print-note print-warning print-error
          format-note format-warning format-error)

  (import (gamma %write) (scheme base) (srfi 28))

  (begin
    (define (format-note message . args)
      (print-note (apply format message args)))

    (define (format-warning message . args)
      (print-warning (apply format message args)))

    (define (format-error message . args)
      (print-error (apply format message args)))))
