#include "./include/GPoint.h"
#include <cmath>

extern float tolerance = 0.25;

class BezierCurve {
public: 
    virtual void setControlPoints(const GPoint[]) = 0;
    virtual GPoint eval(float) = 0;
};

class QuadBezierCurve : public BezierCurve {
public:
    void setControlPoints(const GPoint pts[]) override {
        for (int i = 0; i < 3; i ++) {
            cpts[i] = pts[i];
        }
    }
    
    GPoint eval(float t) override {
        return cpts[0] * pow((1 - t), 2) + 2 * cpts[1] * t * (1 - t) + cpts[2] * pow(t, 2);
    }

private:
    GPoint cpts[3];
};

class CubicBezierCurve : public BezierCurve {
public:
    void setControlPoints(const GPoint pts[]) override {
        for (int i = 0; i < 4; i ++) {
            cpts[i] = pts[i];
        }
    }
    
    GPoint eval(float t) override {
        return cpts[0] * pow((1 - t), 3) 
            + 3 * cpts[1] * t * pow((1 - t), 2) 
            + 3 * cpts[2] * (1 - t) * pow(t, 2)
            + pow(t, 3) * cpts[3];
    }

private:
    GPoint cpts[4];
};

// Cubic Test data:
// GPoint pts1[] = {GPoint({-1, 4}), GPoint({-1, 2}), GPoint({7, 2}), GPoint({7, 6})};
// CubicBezierCurve cbc;
// cbc.setControlPoints(pts1);
// GPoint ans1 = cbc.eval(0.1); // (-0.776, 3.462)
// GPoint ans1 = cbc.eval(0.5); // (3, 2.75)
// https://www.desmos.com/calculator/ebdtbxgbq0