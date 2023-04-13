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
public:
    TriTexShader(GPoint vertexTexCoords[3], GPoint vertices[3], GShader* shaderProvider) : shaderProvider(shaderProvider) {
        std::copy(vertexTexCoords, vertexTexCoords + 3, TexCoords);
        GMatrix((vertices[1].fX - vertices[0].fX), (vertices[2].fX - vertices[0].fX), vertices[0].fX, 
        (vertices[1].fY - vertices[0].fY), (vertices[2].fY - vertices[0].fY), vertices[0].fY).invert(&deviceToBarycentric);
        GMatrix tex_inv;
        GMatrix((TexCoords[1].fX - TexCoords[0].fX), (TexCoords[2].fX - TexCoords[0].fX), TexCoords[0].fX,
        TexCoords[1].fY - TexCoords[0].fY, TexCoords[2].fY - TexCoords[0].fY, TexCoords[0].fY).invert(&tex_inv);
        m = GMatrix::Concat(deviceToBarycentric, tex_inv);
    }

    bool isOpaque() {
        return shaderProvider->isOpaque();
    }

    bool setContext(const GMatrix& ctm) {
        return shaderProvider->setContext(ctm * m);
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        shaderProvider->shadeRow(x, y, count, row);
    }
private:
    GShader* shaderProvider;
    GMatrix m;
    GMatrix deviceToBarycentric;
    GPoint TexCoords[3];
};

class TriColorTexShader : public GShader {
public:
    TriColorTexShader(TriTexShader* texShader, TriColorShader* colorShader) : 
    ts(texShader), cs(colorShader) {}

    bool isOpaque() {
        return ts->isOpaque() && cs->isOpaque();
    }

    bool setContext(const GMatrix& ctm) {
        return ts->setContext(ctm) && cs->setContext(ctm);
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        GPixel* texRow = new GPixel[count];
        ts->shadeRow(x, y, count, texRow);
        cs->shadeRow(x, y, count, row);
        for (int i = 0; i < count; i ++) {
            // ans[i] = (row[i]/255) * (texRow[i]/255) * 255
            row[i] = row[i] * texRow[i] / 255; 
        }
    }
private:
    TriTexShader* ts;
    TriColorShader* cs;
};