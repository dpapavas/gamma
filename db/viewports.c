#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"

#define PRINT_GL_INTEGER(NAME)                  \
    do {                                        \
        GLint i;                                \
        glGetIntegerv(NAME, &i);                \
        printf(#NAME ": %d\n", i);              \
    } while(0)

static void multiply_matrix_4(
    const GLfloat A[16], const GLfloat B[16], GLfloat R[16])
{
    R[0] = A[0] * B[0] + A[1] * B[4] + A[2] * B[8] + A[3] * B[12];
    R[1] = A[0] * B[1] + A[1] * B[5] + A[2] * B[9] + A[3] * B[13];
    R[2] = A[0] * B[2] + A[1] * B[6] + A[2] * B[10] + A[3] * B[14];
    R[3] = A[0] * B[3] + A[1] * B[7] + A[2] * B[11] + A[3] * B[15];

    R[4] = A[4] * B[0] + A[5] * B[4] + A[6] * B[8] + A[7] * B[12];
    R[5] = A[4] * B[1] + A[5] * B[5] + A[6] * B[9] + A[7] * B[13];
    R[6] = A[4] * B[2] + A[5] * B[6] + A[6] * B[10] + A[7] * B[14];
    R[7] = A[4] * B[3] + A[5] * B[7] + A[6] * B[11] + A[7] * B[15];

    R[8] = A[8] * B[0] + A[9] * B[4] + A[10] * B[8] + A[11] * B[12];
    R[9] = A[8] * B[1] + A[9] * B[5] + A[10] * B[9] + A[11] * B[13];
    R[10] = A[8] * B[2] + A[9] * B[6] + A[10] * B[10] + A[11] * B[14];
    R[11] = A[8] * B[3] + A[9] * B[7] + A[10] * B[11] + A[11] * B[15];

    R[12] = A[12] * B[0] + A[13] * B[4] + A[14] * B[8] + A[15] * B[12];
    R[13] = A[12] * B[1] + A[13] * B[5] + A[14] * B[9] + A[15] * B[13];
    R[14] = A[12] * B[2] + A[13] * B[6] + A[14] * B[10] + A[15] * B[14];
    R[15] = A[12] * B[3] + A[13] * B[7] + A[14] * B[11] + A[15] * B[15];
}

// Document: program

// # Viewports

// Viewports are rectangular subdvisions of a window, each showing one
// particular object.  Every viewport has a name as does every object
// and by convention, when an object is loaded, it is displayed in
// every viewport with a matching name.

// When a viewport is created, it is assigned its default name, which
// is the textual representation of its index.  This allows them to be
// used straight away, by outputting to numeric targets, e.g. with
// something like `#>1`.  Alternatively, their name can be set to that
// of a particular output with the `target` command, dedicating them
// to displaying the object it produces.

// Here, we're mostly concerned with creating and manipulating them,
// along with their viewing and projection characteristics.  For
// details on loading the actual geometry, ref: Refreshing Object
// Geometry.  For details on rendering, ref: Refreshing Window
// Contents.

// ## Viewing and Projection

// The following functions manipulate the viewing and projection
// transformations of a viewport.

void pan_viewport(struct viewport *v, float x, float y)
{
    v->translation[0] += x * v->rotation[0] + y * v->rotation[4];
    v->translation[1] += x * v->rotation[1] + y * v->rotation[5];
    v->translation[2] += x * v->rotation[2] + y * v->rotation[6];
    v->stale.projection = true;
}

void translate_viewport(struct viewport *v, float x, float y, float z)
{
    if (isnan(x) || isnan(y) || isnan(z)) {
        if (v->object) {
            auto b = v->object->bounds;
            v->translation[0] = (b[0] + b[3]) / 2.0f;
            v->translation[1] = (b[1] + b[4]) / 2.0f;
            v->translation[2] = (b[2] + b[5]) / 2.0f;
        } else {
            v->translation[0] = 0;
            v->translation[1] = 0;
            v->translation[2] = 0;
        }
    } else {
        v->translation[0] += x;
        v->translation[1] += y;
        v->translation[2] += z;
    }

    v->stale.projection = true;
}

void zoom_viewport(struct viewport *v, float zeta)
{
    if (isnan(zeta)) {
        v->zoom = DEFAULT_VIEWPORT_ZOOM;
    } else {
        v->zoom += zeta;
    }

    v->stale.projection = true;
}

// Rotation matrices are built iteratively.  For every step of the
// mouse cursor, we calculate the "infinitesimal" rotation matrix
// corresponding to that step and concatenate it with the viewport's
// current rotation.

void rotate_viewport(struct viewport *v, float alpha, float beta, float gamma)
{
    if (alpha == 0.0f && beta == 0.0f && gamma == 0.0f) {
        return;
    }

    if (isnan(alpha) || isnan(beta) || isnan(gamma)) {
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                v->rotation[i * 4 + j] = (i == j);
            }
        }
    } else {
        const GLfloat cosalpha = cosf(alpha), sinalpha = sinf(alpha);
        const GLfloat cosbeta = cosf(beta), sinbeta = sinf(beta);
        const GLfloat cosgamma = cosf(gamma), singamma = sinf(gamma);

        // This rotation matrix corresponds to y-x-z Tait-Bryan angles
        // $\alpha$, $\beta$, $\gamma$ i.e. $R_y(\alpha) R_x(\beta)
        // R_z(\gamma)$.

        const GLfloat M[16] = {
            cosalpha * cosgamma + sinalpha * sinbeta * singamma,
            cosgamma * sinalpha * sinbeta - cosalpha * singamma,
            cosbeta * sinalpha,
            0.0f,

            cosbeta * singamma, cosbeta * cosgamma, -sinbeta, 0.0f,

            cosalpha * sinbeta * singamma - cosgamma * sinalpha,
            cosalpha * cosgamma * sinbeta + sinalpha * singamma,
            cosalpha * cosbeta,
            0.0f,

            0.0f, 0.0f, 0.0f, 1.0f
        };

        GLfloat R[16];

        multiply_matrix_4(M, v->rotation, R);
        memcpy(v->rotation, R, sizeof(v->rotation));
    }

    v->stale.projection = true;
}

// Here we update the overall transformation matrix of the viewport.

void refresh_viewport(struct viewport *v)
{
    // Zooming needs to be handled differently in orthographic and
    // perspective projections.  In the former, changing the distance
    // between the camera and the object does nothing and we have to
    // change the parameters of the projection instead.  In the
    // latter, we can do both, although the resulting zoom will
    // differ.

    // We define a zooming factor $\zeta$, which roughly corresponds
    // to the percentage of the projection plane (the viewport) the
    // viewed object would take up.  More precisely, $\zeta = 1$
    // should result in the object's AABB filling up the viewport
    // along one dimension, without exceeding it in the other.

    const GLfloat zeta = v->zoom;

    // In other words, the zoom factor depends on both the shape of
    // the AABB and that of the viewport.  We capture the relationship
    // by calculating the object's effective dimension as:

    const GLfloat a = (GLfloat)(v->top - v->bottom) / (v->right - v->left);
    const GLfloat w = v->object->bounds[3] - v->object->bounds[0];
    const GLfloat h = v->object->bounds[4] - v->object->bounds[1];
    const GLfloat d = v->object->bounds[5] - v->object->bounds[2];

    const GLfloat dim = fmaxf(w, h / a);

    // Now we can turn to the calculation of the projection matrix.

    const GLfloat n = v->near, f = v->far, nf = n - f;

    GLfloat rho, P[16] = {};

    if (v->projection == ORTHOGRAPHIC) {
        // For orthographic projection, we use `dim` to define the
        // viewing volume.  In order to achieve a zoom factor of
        // $\zeta$ we need to make the viewport width equal to
        // $\frac{dim}{\zeta}$.

        // The relevant components of the projection matrix are the
        // first two components of the diagonal (where the viewport
        // width is in the denominator, so we end up with
        // $\zeta\over{dim}$).

        P[0] = 2.0f * zeta / dim;
        P[5] = 2.0f * zeta / dim / a;
        P[10] = 2.0f / nf;
        P[11] = (n + f) / nf;
        P[15] = 1.0f;

        // Although camera distance is immaterial with respect to the
        // projected image, we still translate the object to place it
        // at the middle of the viewing volume along the Z axis.

        rho = (n + f) / 2.0f;
    } else {
        assert(v->projection == PERSPECTIVE);

        // Given the FOV half-angle $\phi_2$, we can compute the right
        // x-coordinate $r$ and top y-coordinate $t$ of the projection
        // plane.  The width and height of the plane is then just $2r$
        // and $2t$ respectively.

        const GLfloat phi_2 = v->angle;
        const GLfloat tanphi_2 = tan(phi_2), r = tanphi_2 * n;
        const GLfloat t = r * a;

        P[0] = n / r;
        P[5] = n / t;
        P[10] = (n + f) / nf;
        P[11] = 2.0f * n * f / nf;
        P[14] = -1.0f;

        // For perspective projection and a given horizontal FOV angle
        // $\phi$ the projected dimension of the object would be
        // $-\frac{n dim}{\rho}$, where $\rho$ the camera distance,
        // whereas the viewport width is $2n\tan{\phi\over{2}}$.  This
        // would make the zoom factor equal to
        // $-\frac{dim}{2\rho\tan{\phi\over{2}}}$, so the required
        // distance to get a zoom factor $\zeta$, would be
        // $-\frac{dim}{2\zeta\tan{\phi\over{2}}}$.

        // We agument this by half the depth of the AABB to make the
        // "front" fill up the viewport as prescribed by the requested
        // zoom factor.  This affords a better match between
        // perspective and orthographic projections.

        rho = dim / (2.0f * zeta * tanphi_2) + d / 2.0f;
    }

    // We apply our translation by $(0, 0, -\rho)$ by
    // post-multiplying the projection matrix $P$ with the
    // required translation matrix.

    for (int i = 0; i < 4; i++) {
        GLfloat *p = &P[4 * i];
        p[3] -= rho * p[2];
    }

    // Finally, we concatenate the projection and rotations matrices
    // and add the viewports translation.

    multiply_matrix_4(P, v->rotation, v->matrix);

    for (int i = 0; i < 4; i++) {
        GLfloat *p = &v->matrix[4 * i];
        p[3] -= (v->translation[0] * p[0]
                 + v->translation[1] * p[1]
                 + v->translation[2] * p[2]);
    }
}
