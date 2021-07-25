(define-library (gamma volumes)
  (include-shared "volumes")
  (export bounding-plane bounding-halfspace bounding-halfspace-interior
          bounding-box bounding-box-boundary bounding-box-interior
          bounding-sphere bounding-sphere-boundary bounding-sphere-interior
          bounding-cylinder bounding-cylinder-boundary
          bounding-cylinder-interior))
