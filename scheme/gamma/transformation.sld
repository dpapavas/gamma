;; -*- mode:scheme; coding: utf-8 -*-

(define-library (gamma transformation)
  (include-shared "transformation")

  (export translation scaling rotation transformation-apply flush
          flush-south flush-north flush-east flush-west flush-bottom
          flush-top)

  (cond-expand
   ((not core-gamma)
    (import (scheme base) (srfi 1))

    (begin
      (define (translation-lambda . args)
        (lambda (x) (transformation-apply (apply translation args) x)))
      (define translation-λ translation-lambda)

      (define (rotation-lambda . args)
        (lambda (x) (transformation-apply (apply rotation args) x)))
      (define rotation-λ rotation-lambda)

      (define (scaling-lambda . args)
        (lambda (x) (transformation-apply (apply scaling args) x)))
      (define scaling-λ scaling-lambda)

      (define (translate part . rest)
        (transformation-apply (apply translation rest) part))

      (define (rotate part . rest)
        (transformation-apply (apply rotation rest) part))

      (define (scale part . rest)
        (transformation-apply (apply scaling rest) part))

      (define (transform x . rest)
        (fold transformation-apply x rest))

      (define (transformation-append . args)
        (fold-right transformation-apply '() args)))

    (export translation-lambda translation-λ rotation-lambda rotation-λ
            scaling-lambda scaling-λ translate rotate scale transform
            transformation-append))))
