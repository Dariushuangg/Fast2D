#ifndef Shader_DEFINED
#define Shader_DEFINED

#include "./include/GShader.h"
#include "./include/GMatrix.h"
#include "./include/GBitmap.h"
#include "./include/GPoint.h"

/// @brief A bitmap shader.
class BitmapShader : public GShader {
public:
    BitmapShader(const GBitmap& ShaderBM, const GMatrix& localInverse, GShader::TileMode tileMode) : 
    ShaderBM(ShaderBM), 
    localInverse(localInverse),
    mode(tileMode) { }

    bool isOpaque() { return ShaderBM.isOpaque(); }
    bool setContext(const GMatrix& ctm) override {
        GMatrix inv_ctm;
        if (!ctm.invert(&inv_ctm)) return false;
        m = GMatrix::Concat(localInverse, inv_ctm);
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        GPoint tmp = m * GPoint{x + 0.5f, y + 0.5f};
        float localX = tmp.fX;
        float localY = tmp.fY;
        float dx = m[0];
        float dy = m[3];
        for (int j = 0; j < count; j ++) {
            float ix = localX + dx * j;
            float iy = localY + dy * j;

            // Clamp
            switch(mode) {
                case GShader::kClamp: {
                    if (ix >= ShaderBM.width()) {
                        ix = ShaderBM.width() - 1;
                    } else if (ix < 0) ix = 0;
                    
                    if (iy >= ShaderBM.height()) {
                        iy = ShaderBM.height() - 1;
                    } else if (iy < 0) iy = 0;
                    break;
                }
                case GShader::kRepeat: {
                    ix = ix / ShaderBM.width();
                    iy = iy / ShaderBM.height();
                    ix = ix - floor(ix);
                    iy = iy - floor(iy);
                    ix = ix * ShaderBM.width();
                    iy = iy * ShaderBM.height();
                    if (ix > ShaderBM.width() - 1) {
                        ix = ShaderBM.width() - 1;
                    }
                    if (iy > ShaderBM.height() - 1) {
                        iy = ShaderBM.height() - 1;
                    }
                    assert(ix < ShaderBM.width());
                    assert(iy < ShaderBM.height()); 
                    break;
                }
                case GShader::kMirror: {
                    ix = ix / (2 * ShaderBM.width());
                    iy = iy / (2 * ShaderBM.height());
                    ix = ix - floor(ix);
                    iy = iy - floor(iy);
                    if (ix <= 0.5) ix = ix * 2 * ShaderBM.width();
                    else {
                        float diff = ix - 0.5;
                        ix = (0.5 - diff) * 2 * ShaderBM.width();
                    }

                    if (iy <= 0.5) iy = iy * 2 * ShaderBM.height();
                    else {
                        float diff = iy - 0.5;
                        iy = (0.5 - diff) * 2 * ShaderBM.height(); 
                    }
                    
                    if (ix > ShaderBM.width() - 1) {
                        ix = ShaderBM.width() - 1;
                    }
                    if (iy > ShaderBM.height() - 1) {
                        iy = ShaderBM.height() - 1;
                    }
                    assert(ix < ShaderBM.width());
                    assert(iy < ShaderBM.height()); 
                    break;
                }
            }
            row[j] = *ShaderBM.getAddr(ix, iy);
        }
    }

private:
    GMatrix m;
    GMatrix localInverse;
    GBitmap ShaderBM;
    GShader::TileMode mode;
};


std::unique_ptr<GShader> GCreateBitmapShader(const GBitmap& ShaderBM,
 const GMatrix& localInverse,
 GShader::TileMode tileMode) {
    return std::unique_ptr<GShader>(new BitmapShader(ShaderBM, localInverse, tileMode));
}

#endif