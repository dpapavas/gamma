;; -*- mode:scheme; coding: utf-8 -*-

(define-library (gamma inexact)
  (cond-expand
   ((not core-gamma)
    (import (scheme base) (scheme inexact))

    (begin
      (define (degrees->radians θ) (* θ 1/180 (acos -1)))
      (define (radians->degrees θ) (/ θ 1/180 (acos -1)))

      (define (cos-degrees x) (cos (degrees->radians x)))
      (define (sin-degrees x) (sin (degrees->radians x)))
      (define (tan-degrees x) (tan (degrees->radians x)))

      (define cos° cos-degrees)
      (define sin° sin-degrees)
      (define tan° tan-degrees)

      (define (acos-degrees x) (radians->degrees (acos x)))
      (define (asin-degrees x) (radians->degrees (asin x)))
      (define (atan-degrees . args) (radians->degrees (apply atan args)))

      (define acos° acos-degrees)
      (define asin° asin-degrees)
      (define atan° atan-degrees))

    (export degrees->radians radians->degrees
            cos-degrees sin-degrees tan-degrees cos° sin° tan°
            acos-degrees asin-degrees atan-degrees acos° asin° atan°))))
