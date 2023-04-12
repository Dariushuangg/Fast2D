#include "./include/GMatrix.h"
#include <cmath>
#include <assert.h>

// Implementation of GMatrix methods

GMatrix::GMatrix() {
    fMat[0] = 1;
    fMat[4] = 1;

    fMat[1] = 0;
    fMat[2] = 0;
    fMat[3] = 0;
    fMat[5] = 0;
}

bool GMatrix::invert(GMatrix* inverse) const {
    float a = (*this)[0];
    float e = (*this)[4];
    float b = (*this)[1];
    float d = (*this)[3];
    float c = (*this)[2];
    float f = (*this)[5];

    float det = a * e - b * d;
    if (!det) return false;

    float i_a = e;
    float i_e = a;
    float i_b = - b;
    float i_d = - d;

    float i_c = - (i_a * c + i_b * f);
    float i_f = - (i_d * c + i_e * f);

    *inverse = GMatrix(i_a/det, i_b/det, i_c/det, i_d/det, i_e/det, i_f/det);
    return true;
}

void GMatrix::mapPoints(GPoint dst[], const GPoint src[], int count) const {
    for (int i = 0; i < count; i ++) {
        dst[i] = GPoint({
            (*this)[0] * src[i].fX + (*this)[1] * src[i].fY + (*this)[2],
            (*this)[3] * src[i].fX + (*this)[4] * src[i].fY + (*this)[5]
        });
    }

}

GMatrix GMatrix::Concat(const GMatrix& a, const GMatrix& b) {
    float n_a = a[0] * b[0] + a[1] * b[3];
    float n_b = a[0] * b[1] + a[1] * b[4];
    float n_d = a[3] * b[0] + a[4] * b[3];
    float n_e = a[3] * b[1] + a[4] * b[4];
    float n_c = a[0] * b[2] + a[1] * b[5] + a[2] * 1;
    float n_f = a[3] * b[2] + a[4] * b[5] + a[5] * 1;
    return GMatrix(n_a, n_b, n_c, n_d, n_e, n_f);
}

GMatrix GMatrix::Translate(float tx, float ty) {
    return GMatrix(1, 0, tx, 0, 1, ty);
}   

GMatrix GMatrix::Scale(float sx, float sy) {
    return GMatrix(sx, 0, 0, 0, sy, 0);
}   

GMatrix GMatrix::Rotate(float radians) {
    // float angle = radians * 113.92405; // 113.92405 = 360 / 3.16
    // return GMatrix(cos(angle), -sin(angle), 0, sin(angle), cos(angle), 0);
    return GMatrix(cos(radians), -sin(radians), 0, sin(radians), cos(radians), 0);
}   