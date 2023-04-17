#include "./include/GPath.h"
#include <algorithm>
using namespace std;

void GPath::ChopQuadAt(const GPoint src[3], GPoint dst[5], float t) {
    dst[0] = src[0];
    dst[4] = src[2];
    dst[1] = (1 - t) * src[0] + t * src[1];
    dst[3] = (1 - t) * src[1] + t * src[2];
    dst[2] = (1 - t) * dst[1] + t * dst[3];
}

void GPath::ChopCubicAt(const GPoint src[4], GPoint dst[7], float t) {
    dst[0] = src[0];
    dst[6] = src[3];
    dst[1] = (1 - t) * src[0] + t * src[1];
    dst[5] = (1 - t) * src[2] + t * src[3];
    GPoint midBC = (1 - t) * src[1] + t * src[2];
    dst[2] = (1 - t) * dst[1] + t * midBC;
    dst[4] = (1 - t) * midBC + t * dst[5];
    dst[3] = (1 - t) * dst[2] + t * dst[4];
}

void GPath::addCircle(GPoint center, float radius, GPath::Direction direction) {
    float tan_pi_8 = 0.4142;
    float tan_pi_4 = 0.7071;

    // scale and transform unit circle
    GMatrix mx = GMatrix::Translate(center.fX, center.fY) * GMatrix::Scale(radius, radius);

    this->moveTo(mx * GPoint({1, 0}));

    //!! for some reason, this is the correct direction!
    if (direction == GPath::kCCW_Direction) {
        GPoint kCWUnitCirclePts[] = {
            // 4
            GPoint({1, -tan_pi_8}), GPoint({tan_pi_4, -tan_pi_4}),
            GPoint({tan_pi_8, -1}), GPoint({0, -1}),
            // 3
            GPoint({-tan_pi_8, -1}), GPoint({-tan_pi_4, -tan_pi_4}),
            GPoint({-1, -tan_pi_8}), GPoint({-1, 0}),
            // 2
            GPoint({-1, tan_pi_8}), GPoint({-tan_pi_4, tan_pi_4}),
            GPoint({-tan_pi_8, 1}), GPoint({0, 1}),
            // 1
            GPoint({tan_pi_8, 1}), GPoint({tan_pi_4, tan_pi_4}),
            GPoint({1, tan_pi_8}), GPoint({1, 0})
        };

        // transform our unit circle in kCWUnitCirclePts by radius and center
        mx.mapPoints(kCWUnitCirclePts, kCWUnitCirclePts, 16); 
        for (int i = 0; i < 8; i++) {
            this->quadTo(kCWUnitCirclePts[2 * i], kCWUnitCirclePts[2 * i + 1]);
        }
    } else {
        //!! This is actually clock-wise, but I think it should be ccw??
        // in fact, kCCWUnitCirclePts is the reverse of the first 15 elements in 
        // kCWUnitCirclePts concat with (1, 0) as the last point (so it's
        // NOT a direct std::reverse). Hardcode this make it faster.
        GPoint kCCWUnitCirclePts[] = {
            // 1
            GPoint({1, tan_pi_8}), GPoint({tan_pi_4, tan_pi_4}),
            GPoint({tan_pi_8, 1}), GPoint({0, 1}),

            // 2
            GPoint({-tan_pi_8, 1}), GPoint({-tan_pi_4, tan_pi_4}),
            GPoint({-1, tan_pi_8}), GPoint({-1, 0}),

            // 3
            GPoint({-1, -tan_pi_8}), GPoint({-tan_pi_4, -tan_pi_4}),
            GPoint({-tan_pi_8, -1}), GPoint({0, -1}),

            // 4
            GPoint({tan_pi_8, -1}), GPoint({tan_pi_4, -tan_pi_4}),
            GPoint({1, -tan_pi_8}), GPoint({1, 0})
        };

        // transform our unit circle in kCCWUnitCirclePts by radius and center
        mx.mapPoints(kCCWUnitCirclePts, kCCWUnitCirclePts, 16);  
        for (int i = 0; i < 8; i++) {
            this->quadTo(kCCWUnitCirclePts[2 * i], kCCWUnitCirclePts[2 * i + 1]);
        }
    }
}

GPoint* mapCirclePoints(GMatrix mx, GPoint p1, GPoint p2) {
    GPoint pts[] = {p1, p2};
    mx.mapPoints(pts, pts, 2);
    return pts;
}

void GPath::addRect(const GRect& rect, Direction direction) {
    GPoint pts[4];
    pts[0] = { rect.fLeft,  rect.fTop };
    pts[1] = { rect.fRight, rect.fTop };
    pts[2] = { rect.fRight, rect.fBottom };
    pts[3] = { rect.fLeft,  rect.fBottom };

    this->moveTo(pts[0]);
    if (direction == Direction::kCW_Direction) {
        this->lineTo(pts[1]);
        this->lineTo(pts[2]);
        this->lineTo(pts[3]);
        this->lineTo(pts[0]);
    } else {
        this->lineTo(pts[3]);
        this->lineTo(pts[2]);
        this->lineTo(pts[1]);
        this->lineTo(pts[0]);
    }
}

/**
 * @brief Add a polygon given by pts[] to the path
 * 
 * @param pts point representation of a polygon
 * @param count number of points given
 */
void GPath::addPolygon(const GPoint pts[], int count) {
    this->moveTo(pts[0]);
    for (int i = 1; i < count; i ++) {
        this->lineTo(pts[i]);
    }
    // this->lineTo(pts[0]);
}

GRect GPath::bounds() const {
    if (this->fPts.size() == 1) return GRect::XYWH(fPts[0].fX, fPts[0].fY, 0, 0); //!! ?????

    float left = 0;
    float right = 0;
    float top = 0;
    float bot = 0;

    for (GPoint p : this->fPts) {
        left = std::min(left, p.fX);
        right = std::max(right, p.fX);
        top = std::min(top, p.fY);
        bot = std::max(bot, p.fY);
    }
    return GRect({left, top, right, bot});
}

void GPath::transform(const GMatrix& ctm) {
    // for (int i = 0; i < this->fPts.size(); i ++) {
    //     printf("Pre-trans pts[0] = (%f, %f) \n", this->fPts[i].fX, this->fPts[i].fY);
    // }
    ctm.mapPoints(this->fPts.data(), this->fPts.size());
    // for (int i = 0; i < this->fPts.size(); i ++) {
    //     printf("Path to draw: pts[%d] = (%f, %f) \n", i, this->fPts[i].fX, this->fPts[i].fY);
    // }
}
