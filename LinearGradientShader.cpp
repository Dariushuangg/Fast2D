#include "./include/GShader.h"
#include "./include/GMatrix.h"
#include "./include/GColor.h"
#include "./GBlenders.h"

class LinearGradientShader : public GShader {
public:
    LinearGradientShader(GPoint p0, GPoint p1, const GColor c[], int count, GShader::TileMode md) {
        mode = md;

        float x11 = p1.fX - p0.fX;
        float x21 = p1.fY - p0.fY;
        float x12 = - x21;
        float x22 = x11;
        bool success = GMatrix({x11, x12, p0.fX, x21, x22, p0.fY}).invert(&T_gradient);
        if (!success) {
            assert(success);
        }

        assert(count < 10); // I pre-alloc color, so must ensure size is correct
        std::copy(c, c + count, colors);
        colors[count] = GColor({1, 1, 1, 1}); 
        numOfColors = count;
    }

    bool isOpaque() {
        for (int i = 0; i < numOfColors - 1; i ++) {
            if (colors[i].a != 1) return false;
        }
        return true;
    }

    bool setContext(const GMatrix& ctm) override {
        GMatrix inv_ctm;
        if (!ctm.invert(&inv_ctm)) return false;
        m = GMatrix::Concat(T_gradient, inv_ctm);
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        GPoint tmp = m * GPoint{x + 0.5f, y + 0.5f};
        float localX = tmp.fX;
        float dx = m[0];
        for (int j = 0; j < count; j ++) {
            float ix = localX + dx * j;
            float cix;
            switch (mode) {
                case GShader::kClamp:
                    cix = clamp(ix); // clamp
                    break;
                case GShader::kRepeat:
                    cix = ix - floor(ix);
                    break;
                case GShader::kMirror:
                    // Just like how we use division + floor as module for kRepeat,
                    // we use the same trick here. 
                    float tmp = ix / 2; 

                    // proportion is between 0~1, where 0~0.5 represent vector (0,0) -> (1,0) 
                    // in shader space, and 0.5~1.0 represent vector (1,0) -> (2,0)
                    float proportion = tmp - floor(tmp); 
                    
                    if (proportion > 0.5) {
                        // mirror = symmetry by 0.5
                        proportion = 1 - proportion;
                    }
                    // cix is now the position in the (0,0) -> (1,0) shader space
                    // since 0~0.5 represent half of the space
                    cix = proportion * 2;
            }

            float fColorIndx = cix * (numOfColors - 1);
            int startColorIdx = std::floor(fColorIndx); //!! Do we need to calculate this every time?
            int endColorIdx = startColorIdx + 1;
            float w = endColorIdx - fColorIndx;
            
            GColor ic = interpolate(colors[startColorIdx], colors[endColorIdx], w); // calculate color
            GPixel premult_ic = Blenders::prepSrcPixel(ic);
            row[j] = premult_ic;
        }
    }

private:
    GColor colors[10]; // pre-allocate to 10
    int numOfColors;
    GMatrix m; // Matrix to use
    GMatrix T_gradient; // Matrix that transform from world space to gradient-line space
    GShader::TileMode mode;

    GColor interpolate(GColor start, GColor end, float w) {
        float ew = 1 - w;
        float nr = start.r * w + end.r * ew;
        float ng = start.g * w + end.g * ew;
        float nb = start.b * w + end.b * ew;
        float na = start.a * w + end.a * ew;
        return GColor({nr, ng, nb, na});
    }

    float clamp(float ix) {
        if (ix < 0) return 0;
        else if (ix > 1) return 1;
        else return ix;
    }
};

class SingleColorShader : public GShader {
public:
    SingleColorShader(GPoint p0, GPoint p1, const GColor c[], int count, GShader::TileMode mode) {
        p = Blenders::prepSrcPixel(c[0]);
    }

    bool isOpaque() {
        return false; // we don't care, always return the prepared pixel
    }

    bool setContext(const GMatrix& ctm) override {
        return true; // we don't care, always return the prepared pixel
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        for (int j = 0; j < count; j ++) {
            row[j] = p;
        }
    }

private:
    GPixel p;
};

class TwoColorLinearGradientShader : public GShader {
public:
    TwoColorLinearGradientShader(GPoint p0, GPoint p1, const GColor c[], int count, GShader::TileMode md) {
        mode = md;

        float x11 = p1.fX - p0.fX;
        float x21 = p1.fY - p0.fY;
        float x12 = - x21;
        float x22 = x11;
        bool success = GMatrix({x11, x12, p0.fX, x21, x22, p0.fY}).invert(&T_gradient);
        if (!success) {
            assert(success);
        }

        colors[0] = c[0];
        colors[1] = c[1];
        left = Blenders::prepSrcPixel(colors[0]);
        right = Blenders::prepSrcPixel(colors[1]);

        numOfColors = 2;
    }

    bool isOpaque() {
        return colors[0].a == 1 && colors[1].a == 1;
    }

    bool setContext(const GMatrix& ctm) override {
        GMatrix inv_ctm;
        if (!ctm.invert(&inv_ctm)) return false;
        m = GMatrix::Concat(T_gradient, inv_ctm);
        return true;
    }

    void shadeRow(int x, int y, int count, GPixel row[]) {
        GPoint tmp = m * GPoint{x + 0.5f, y + 0.5f};
        float localX = tmp.fX;
        float dx = m[0];
        for (int j = 0; j < count; j ++) {
            float ix = localX + dx * j;
            float cix;
            switch(mode) {
                case GShader::kClamp: {
                    cix = ix;
                    if (ix >= 1.0f) {
                        row[j] = right;
                        return;
                    }
                    if (ix <= 0.0f) {
                        row[j] = left;
                        return;
                    }
                    break;
                }
                case GShader::kRepeat: {
                    cix = ix - floor(ix);
                    break;
                }
                case GShader::kMirror: {
                    float tmp = ix / 2; 
                    float proportion = tmp - floor(tmp); 
                    if (proportion > 0.5) {
                        proportion = 1 - proportion;
                    }
                    cix = proportion * 2;
                }
            }
                        
            GColor ic = interpolate(cix); // calculate color
            GPixel premult_ic = Blenders::prepSrcPixel(ic);
            row[j] = premult_ic;
        }
    }

private:
    GColor colors[10]; // pre-allocate to 10
    int numOfColors;
    GMatrix m; // Matrix to use
    GMatrix T_gradient; // Matrix that transform from world space to gradient-line space
    GPixel left;
    GPixel right;
    GShader::TileMode mode;

    GColor interpolate(float ix) {
        float c = 1.0f - ix;
        float nr = colors[0].r * c + colors[1].r * ix;
        float ng = colors[0].g * c + colors[1].g * ix;
        float nb = colors[0].b * c + colors[1].b * ix;
        float na = colors[0].a * c + colors[1].a * ix;
        return GColor({nr, ng, nb, na});
    }
};

std::unique_ptr<GShader> GCreateLinearGradient(GPoint p0, GPoint p1, const GColor colors[], int count, GShader::TileMode mode){
    if (count == 1) return std::unique_ptr<GShader>(new SingleColorShader(p0, p1, colors, count, mode));
    if (count == 2) return std::unique_ptr<GShader>(new TwoColorLinearGradientShader(p0, p1, colors, count, mode));
    return std::unique_ptr<GShader>(new LinearGradientShader(p0, p1, colors, count, mode));
}
