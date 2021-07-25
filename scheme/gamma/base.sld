;; -*- mode:scheme; coding: utf-8 -*-

(define-library (gamma base)
  (include-shared "base")

  (import (scheme base))

  (begin
    (define-syntax define-option
      (syntax-rules ()
        ((_ sym exp) (%define-option (quote sym) exp))))

    (define-syntax define-output
      (syntax-rules ()
        ((_ sym exp) (define sym (output (symbol->string (quote sym)) exp)))))

    (define-syntax ?
      (syntax-rules ()
        ((_ exp ...) (let ((s (exp ...)))
                       (output s)
                       s)))))

  (export define-option define-output output ?
          set-projection-tolerance! set-curve-tolerance! set-sine-tolerance!
          point plane)

  (cond-expand
   ((not core-gamma)
    (import (scheme case-lambda) (scheme write)
            (srfi 1)
            (chibi match))

    (begin
      (define-syntax λ lambda)
      (define-syntax case-λ case-lambda)

      (define tile
        (case-lambda
         ((l n m) (cond ((null? l) l)
                        ((< (length l) n) '())
                        (else
                         (let ((a (take l n))
                               (b (drop l m)))
                           (cons a (tile b n m))))))
         ((l n) (tile l n n))))

      (define range
        (case-lambda
         ((a b delta) (if (eq? (< a b) (positive? delta))
                          (cons a (range (+ a delta) b delta))
                          (list b)))
         ((b delta) (range 0 b delta))))

      (define linear-partition
        (case-lambda
         ((a b n) (range a b (/ (- b a) n)))
         ((b n) (range 0 b (/ b n)))))

      (define-syntax with-curve-tolerance
        (syntax-rules ()
          ((_ new exp ...) (let* ((old (set-curve-tolerance! new))
                                  (out (begin exp ...)))
                             (set-curve-tolerance! old)
                             out))))

      (define-syntax with-projection-tolerance
        (syntax-rules ()
          ((_ new exp ...) (let* ((old (set-projection-tolerance! new))
                                  (out (begin exp ...)))
                             (set-projection-tolerance! old)
                             out))))

      (define-syntax with-sine-tolerance
        (syntax-rules ()
          ((_ new exp ...) (let* ((old (set-sine-tolerance! new))
                                  (out (begin exp ...)))
                             (set-sine-tolerance! old)
                             out))))

      (define-syntax %list-for
        (syntax-rules ()
          ((_ out () exp ...) (cons (begin exp ...) out))
          ((_ out ((x xs) rest ...) exp ...)
           (let loop ((x_i xs))
             (if (null? x_i) out
                 (let ((x (car x_i)))
                   (%list-for (loop (cdr x_i)) (rest ...) exp ...)))))))

      (define-syntax list-for
        (syntax-rules ()
          ((_ ((x xs) rest ...) exp ...)
           (%list-for '() ((x xs) rest ...) exp ...))))

      (define-syntax %match-for
        (syntax-rules ()
          ((_ out () exp ...) (cons (begin exp ...) out))
          ((_ out ((x xs) rest ...) exp ...)
           (let loop ((x_i xs))
             (if (null? x_i) out
                 (match-let ((x (car x_i)))
                            (%match-for (loop (cdr x_i)) (rest ...) exp ...)))))))

      (define-syntax match-for
        (syntax-rules ()
          ((_ ((x xs) rest ...) exp ...)
           (%match-for '() ((x xs) rest ...) exp ...))))

      (define-syntax %partial
        (syntax-rules (_)

          ;; Construct final procedure.

          ((%partial (name ...) (proc arg ...))
           (lambda (name ... . rest) (apply proc arg ... rest)))

          ;; Accumulate one argument or slot.

          ((%partial (name ...) (arg ...) _ . rest)
           (%partial (name ... x) (arg ... x) . rest))
          ((%partial (name ...) (arg ...) a . rest)
           (%partial (name ...) (arg ... a) . rest))))

      (define-syntax partial
        (syntax-rules ()
          ((partial . args)
           (%partial () () . args))))

      (define-syntax %~>
        (syntax-rules (_)
          ;; value fn p args-in args-out rest

          ;; Evaluate final step.

          ((%~> value () () () () ()) value)
          ((%~> value fn #true () (out ...) ())
           (fn out ...))
          ((%~> value fn #false () (out ...) ())
           ((fn out ...) value))

          ;; Evaluate.

          ((%~> value fn #true () (out ...) rest)
           (%~> (fn out ...) () () () () rest))
          ((%~> value fn #false () (out ...) rest)
           (%~> ((fn out ...) value) () () () () rest))

          ;; Load next (potentially bare function) step.

          ((%~> value () () () () ((next-fn next-arg ...) rest ...))
           (%~> value next-fn #false (next-arg ...) () (rest ...)))
          ((%~> value () () () () (next-fn rest ...))
           (%~> value next-fn #false (_) () (rest ...)))

          ;; Replace _ in arguments with value.

          ((%~> value fn p (_ arg ...) (out ...) rest)
           (%~> value fn #true (arg ...) (out ... value) rest))
          ((%~> value fn p (first arg ...) (out ...) rest)
           (%~> value fn p (arg ...) (out ... first) rest))))

      (define-syntax ~>
        (syntax-rules (_)
          ((~> value steps ...)
           (%~> value () () () () (steps ...)))))

      (define-syntax %when~>
        (syntax-rules (_)
          ;; value fn p args-in args-out rest

          ;; Evaluate final step.

          ((%when~> value () () () () ()) value)
          ((%when~> value fn #true () (out ...) ())
           (fn out ...))
          ((%when~> value fn #false () (out ...) ())
           ((fn out ...) value))

          ;; Evaluate.

          ((%when~> value fn #true () (out ...) rest)
           (%when~> (fn out ...) () () () () rest))
          ((%when~> value fn #false () (out ...) rest)
           (%when~> ((fn out ...) value) () () () () rest))

          ;; Load final step.

          ((%when~> value () () () () (cond? (next-fn next-arg ...)))
           (if cond?
               (%when~> value next-fn #false (next-arg ...) () ())
               value))
          ((%when~> value () () () () (cond? next-fn))
           (if cond?
               (%when~> value next-fn #false (_) () ())
               value))

          ;; Load next (potentially bare function) step.

          ((%when~> value () () () () (cnd (next-fn next-arg ...) rest ...))
           (if cnd
               (%when~> value next-fn #false (next-arg ...) () (rest ...))
               (%when~> value () () () () (rest ...))))
          ((%when~> value () () () () (cnd next-fn rest ...))
           (if cnd
               (%when~> value next-fn #false (_) () (rest ...))
               (%when~> value () () () () (rest ...))))

          ;; Replace _ in arguments with value.

          ((%when~> value fn p (_ arg ...) (out ...) rest)
           (%when~> value fn #true (arg ...) (out ... value) rest))
          ((%when~> value fn p (first arg ...) (out ...) rest)
           (%when~> value fn p (arg ...) (out ... first) rest))))

      (define-syntax when~>
        (syntax-rules (_)
          ((when~> value steps ...)
           (%when~> value () () () () (steps ...)))))

      (define-syntax lambda~>
        (syntax-rules ()
          ((lambda~> steps ...) (lambda (x) (~> x steps ...)))))

      (define-syntax λ~> lambda~>))

    (export λ case-λ tile range linear-partition with-curve-tolerance
            with-sine-tolerance list-for match-for partial ~> when~> lambda~>
            λ~>))))
