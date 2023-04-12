#include "./include/GShader.h"
#include "./include/GMatrix.h"
#include "./include/GColor.h"
#include "./GBlenders.h"

class TriColorShader : public GShader {
public:
    TriColorShader(GColor vertexColors[3], GPoint vertices[3]) {
        std::copy(vertexColors, vertexColors + 3, colors);
        GMatrix((vertices[1].fX - vertices[0].fX), (vertices[2].fX - vertices[0].fX), vertices[0].fX, 
        (vertices[1].fY - vertices[0].fY), (vertices[2].fY - vertices[0].fY), vertices[0].fY).invert(&deviceToBarycentric);
        opaque = vertexColors[0].a == 1.0f && vertexColors[1].a == 1.0f && vertexColors[2].a == 1.0f;
    }

    bool isOpaque() {
        return opaque;
    }

    bool setContext(const GMatrix& ctm) {
        GMatrix inv_ctm;
        if (!ctm.invert(&inv_ctm)) return false;
        m = GMatrix::Concat(deviceToBarycentric, inv_ctm);
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        GColor DC1 = colors[1] - colors[0];
        GColor DC2 = colors[2] - colors[0];
        GColor DC = m[0] * DC1 + m[3] * DC2; // DC = a * DC1 + d * DC2
        GPoint barycentric = m * GPoint{x + 0.5f, y + 0.5f};
        GColor C = barycentric.fX * DC1 + barycentric.fY * DC2 + colors[0];
        for (int i = 0; i < count; i ++) {
            row[i] = Blenders::prepSrcPixel(C);
            C += DC;
        }
    }

private:
    GMatrix m; 
    GMatrix deviceToBarycentric; // M^-1 in class note
    GColor colors[3];
    bool opaque;
};

class TriTexShader : public GShader {
    bool isOpaque() {
        return true;
    }

    bool setContext(const GMatrix& ctm) {
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        return;
    }
};

class TriColorTexShader : public GShader {
    bool isOpaque() {
        return true;
    }

    bool setContext(const GMatrix& ctm) {
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        return;
    }
};