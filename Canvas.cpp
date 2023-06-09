/**
 * @author Darius Huang
 * @copyright Copyright (c) 2023
 * 
 */

#include "./include/GPath.h"
#include "./include/GCanvas.h"
#include "./include/GBitmap.h"
#include "./include/GShader.h"
#include"./include/GPaint.h"
#include "./include/GBlendMode.h"
#include "./include/GRect.h"
#include <vector>
#include <stack>
#include "./GEdge.h"
#include "./GBlenders.h"
#include "./BezierCurve.h"
#include "./TriangleShaders.h"

using namespace std;

class Canvas : public GCanvas {
public:
    Canvas(const GBitmap& bitmap) : fDevice(bitmap), blenders(Blenders()) {
        matrixStack.push(GMatrix());
    }

    /////////////////////////////////////////////////////////////////////////// 
    // Matrix stack operations
    void save() {
        matrixStack.push(matrixStack.top()); 
    }

    void restore() {
        matrixStack.pop();
    }

    void concat(const GMatrix& matrix) {
        GMatrix top = matrixStack.top();
        GMatrix newTop = GMatrix::Concat(top, matrix);
        matrixStack.pop();
        matrixStack.push(newTop);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Draw calls
    
    /// @brief Draw triangle meshes based upon the provided payload.
    /// @param verts List of vertices coordinates.
    /// @param colors List of colors; Could be null if no color is used.
    /// @param texs List of texture UV; Could be null if no texture is used.
    /// @param count Number of triangles.
    /// @param indices List of indices into the payload arrays.
    /// @param textureShader The texture provider, if texture is used.
    void drawMesh(const GPoint verts[], const GColor colors[], const GPoint texs[],
                          int count, const int indices[], const GPaint& textureShader) override 
    {
        bool hasColor = colors != nullptr;
        bool hasTex = texs != nullptr;
        if (!hasColor && !hasTex) {
            return;
        } else if (hasColor && !hasTex) {
            drawColorMesh(verts, colors, count, indices);
        } else if (!hasColor && hasTex) {
            drawTexMesh(verts, texs, count, indices, textureShader);
        } else {
            drawColorTexMesh(verts, colors, texs, count, indices, textureShader);
        }
    }

    /// @brief Draw a quad mesh based upon the provided payload.
    /// @param verts List of vertices coordinates.
    /// @param colors List of colors; Could be null if no color is used.
    /// @param texs List of texture UV; Could be null if no texture is used.
    /// @param level Level of details for subdivisions.
    /// @param paint Paint of the quad.
    void drawQuad(const GPoint verts[4], const GColor colors[4], const GPoint texs[4],
                          int level, const GPaint& paint)
    {
        GPoint** i_vertices = bilinearInterpolatePayload(verts, level);
        GPoint** i_textures = bilinearInterpolatePayload(texs, level);
        GColor** i_colors = bilinearInterpolatePayload(colors, level);

        // construct the indices array, which is identical for all payloads 
        int numOfTriangles = (level + 1) * (level + 1) * 2;
        int* indices = new int[numOfTriangles * 3];
        int idx = 0;
        for (int t = 0; t < level + 1; t ++) {
            for (int s = 0; s < level + 1; s ++) {
                // top left triangle
                indices[idx] = t * (level + 2) + s;
                indices[idx+1] = indices[idx] + 1;
                indices[idx+2] = (t + 1) * (level + 2) + s;
                // bot right triangle
                indices[idx+3] = indices[idx+2];
                indices[idx+4] = indices[idx+1];
                indices[idx+5] = (t + 1) * (level + 2) + (s + 1);

                idx += 6;
            }
        }

        // construct the vert[], colors[], texs[] arrays by flattening
        GPoint* flat_i_vertices = new GPoint[(level + 2) * (level + 2)];
        for (int i = 0; i < level + 2; i ++) {
            for (int j = 0; j < level + 2; j ++) {
                flat_i_vertices[i * (level + 2) + j] = i_vertices[i][j];
            }
        }

        GPoint* flat_i_textures;
        if (i_textures != nullptr) {
            flat_i_textures = new GPoint[(level + 2) * (level + 2)];
            for (int i = 0; i < level + 2; i ++) {
                for (int j = 0; j < level + 2; j ++) {
                    flat_i_textures[i * (level + 2) + j] = i_textures[i][j];
                }
            }
        } else flat_i_textures = nullptr;

        GColor* flat_i_colors;
        if (i_colors != nullptr) {
            flat_i_colors = new GColor[(level + 2) * (level + 2)];
            for (int i = 0; i < level + 2; i ++) {
                for (int j = 0; j < level + 2; j ++) {
                    flat_i_colors[i * (level + 2) + j] = i_colors[i][j];
                }
            }
        } else flat_i_colors = nullptr;

        drawMesh(flat_i_vertices, flat_i_colors, flat_i_textures, numOfTriangles, indices, paint);
    }

    /// @brief Draw a rectangle given the shape and the paint.
    /// @param rect ~
    /// @param paint ~
    void drawRect(const GRect& rect, const GPaint& paint) {
        GPoint pts[4];
        pts[0] = { rect.fLeft,  rect.fTop };
        pts[1] = { rect.fRight, rect.fTop };
        pts[2] = { rect.fRight, rect.fBottom };
        pts[3] = { rect.fLeft,  rect.fBottom };
        
        drawConvexPolygon(pts, 4, paint);
    };

    /// @brief Fill the entire canvas with the specified color, using the specified blendmode.
    /// @param paint Paint of the screen.
    void drawPaint(const GPaint& paint) override {
        rowBlender rb = blenders.getBlender(paint.getBlendMode());
        GShader* shaderptr = paint.getShader();
        if (shaderptr == nullptr) {
            // No shader is used
            GPixel srcPixel = Blenders::prepSrcPixel(paint.getColor());
            for (int r = 0; r < fDevice.height(); r ++) {
                rb(0, fDevice.width(), &srcPixel, false, fDevice.getAddr(0, r));
            }
        } else {
            shaderptr->setContext(matrixStack.top()); 
            int rowBytes = fDevice.rowBytes() >> 2;
            for (int r = 0; r < fDevice.height(); r ++) {
                shaderptr->shadeRow(0, r, fDevice.width(), fDevice.pixels()+ r * rowBytes);
            }
        }
    }

    /// @brief Draw a path using the specified paint.
    /// @param cpath ~
    /// @param paint ~
    void drawPath(const GPath& cpath, const GPaint& paint) {
        // Set the shader's context, if a shader is used.
        GShader* shaderptr = paint.getShader();
        bool hasShader = (shaderptr != nullptr);
        if (hasShader) {
            shaderptr->setContext(matrixStack.top());
        }

        // Transform the points from model space to device space using the ctm
        GPath path = cpath;
        path.transform(matrixStack.top());

        vector<GEdge> edges = assembleEdges(path);

        std::sort(edges.begin(), edges.end(), [](const GEdge& e1, const GEdge& e2){
            if (e1.top == e2.top) {
                float x1 = e1.m * (e1.top + 0.5) + e1.b;
                float x2 = e2.m * (e2.top + 0.5) + e2.b;
                return x1 < x2;
            } else return e1.top < e2.top;
        }); 
        
        for (int y = 0; y < fDevice.height();) {
            if (edges.size() == 0) return;

            // Blit the scan line
            int i = 0;
            int w = 0;
            int left, right = 0;
            while (i < static_cast<int>(edges.size()) && edges[i].top <= y) {
                GEdge currEdge = edges[i];
                int x = GRoundToInt(currEdge.m * ((float)y + 0.5) + currEdge.b); //!! Refactor into a GEdge method
                if (w == 0) {
                    left = x;
                }
                w += currEdge.orientation;
                if (w == 0) {
                    // the loop is closed
                    right = x;
                    fillRow(left, right, y, paint, hasShader);
                }
                if (edgeHasExpired(currEdge, y, edges)) {
                    // printf("erasing edge at index %d; edges.size = %d \n", i, edges.size());
                    edges.erase(edges.begin() + i);
                } else {
                    i++;
                }
            }

            y++;

            // Find active edges
            // move index to include edges that will (in the next) be valid
            while (i < static_cast<int>(edges.size()) && isActive(edges[i], y)) {
                i++;
            }

            // Resort active edges
            resortByCurrX(edges, y, i);

        }
    }

    /// @brief Draw any convex polygon.
    /// @param points Vertices of the polygon.
    /// @param count Number of vertices.
    /// @param paint Paint to fill the polygon with.
    void drawConvexPolygon(const GPoint points[], int count, const GPaint& paint) {    
        assert(count >= 0);
        // Set the shader's context, if a shader is used.
        GShader* shaderptr = paint.getShader();
        bool hasShader = (shaderptr != nullptr);
        if (hasShader) {
            shaderptr->setContext(matrixStack.top());
        }

        // Transform the points from model space to device space using the ctm
        GPoint* transformedPoints = new GPoint[count];
        matrixStack.top().mapPoints(transformedPoints, points, count);

        // Prepare edges
        vector<GEdge> edges = assembleEdges(transformedPoints, count);
        delete[] transformedPoints; // Prevent memory leak
        std::sort(edges.begin(), edges.end(), [](const GEdge& e1, const GEdge& e2){
            if (e1.top == e2.top) {
            return e1.bot < e2.bot;
            } else return e1.top < e2.top;
        }); // sort by Y
        for (int y = 0; y < fDevice.height(); y++) {
            if (edges.empty() || edges.size() == 1) break;
            // Pick edges with the smallest y value (closer to the top of screen)
            GEdge e1 = edges[0]; 
            GEdge e2 = edges[1];
            if (e2.top < e1.top) {
                GEdge temp = e2;
                e2 = e1;
                e1 = temp;
            }
            if (y < e1.top || y > e2.bot) continue;
            // Calculate left and right-most pixel covered by the shape
            int idx1 = GRoundToInt(e1.m * ((float)y + 0.5) + e1.b);
            int idx2 = GRoundToInt(e2.m * ((float)y + 0.5) + e2.b);

            // Fill the entire row of pixel between left and right index
            if (idx1 == idx2) { /* don't draw */ }
            else if (idx1 < idx2) fillRow(idx1, idx2 - 1, y, paint, hasShader);
            else fillRow(idx2, idx1 - 1, y, paint, hasShader);

            // Retire expired edges by removing them; We maintain the invariance
            // that edges with smallest y always has the smallest index in the array
            if (edgeHasExpired(e2, y, edges)) edges.erase(edges.begin() + 1);
            if (edgeHasExpired(e1, y, edges)) edges.erase(edges.begin());
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

private:
    GBitmap fDevice;
    Blenders blenders;
    stack<GMatrix> matrixStack;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void drawColorMesh(const GPoint vertices[], const GColor colors[], int count, const int indices[]) {
        GPoint verts[3];
        GColor cols[3];
        for (int i = 0; i < 3 * count; i += 3) {
            verts[0] = vertices[indices[i]];
            verts[1] = vertices[indices[i+1]];
            verts[2] = vertices[indices[i+2]];

            cols[0] = colors[indices[i]];
            cols[1] = colors[indices[i+1]];
            cols[2] = colors[indices[i+2]];

            std::unique_ptr<GShader> cs = GCreateTriColorShader(cols, verts);
            drawConvexPolygon(verts, 3, GPaint(cs.get()));
        }
    }

    void drawTexMesh(const GPoint vertices[], const GPoint textures[],
     int count, const int indices[], const GPaint& textureShader) 
    {
        GPoint verts[3];
        GPoint texs[3];
        for (int i = 0; i < 3 * count; i += 3) {
            verts[0] = vertices[indices[i]];
            verts[1] = vertices[indices[i+1]];
            verts[2] = vertices[indices[i+2]];

            texs[0] = textures[indices[i]];
            texs[1] = textures[indices[i+1]];
            texs[2] = textures[indices[i+2]];

            std::unique_ptr<GShader> ts 
                        = GCreateTriTexShader(texs, verts, textureShader.getShader());
            drawConvexPolygon(verts, 3, GPaint(ts.get()));
        }
    }

    void drawColorTexMesh(const GPoint vertices[], const GColor colors[], const GPoint textures[],
                          int count, const int indices[], const GPaint& textureShader)
    {
        GPoint verts[3];
        GPoint texs[3];
        GColor cols[3];
        for (int i = 0; i < 3 * count; i += 3) {
            verts[0] = vertices[indices[i]];
            verts[1] = vertices[indices[i+1]];
            verts[2] = vertices[indices[i+2]];

            texs[0] = textures[indices[i]];
            texs[1] = textures[indices[i+1]];
            texs[2] = textures[indices[i+2]];

            cols[0] = colors[indices[i]];
            cols[1] = colors[indices[i+1]];
            cols[2] = colors[indices[i+2]];

            std::unique_ptr<GShader> ts 
                        = GCreateTriTexShader(texs, verts, textureShader.getShader());
            std::unique_ptr<GShader> cs = GCreateTriColorShader(cols, verts);
            std::unique_ptr<GShader> tcs 
                        = GCreateTriColorTexShader((TriTexShader*)(ts.get()),
                            (TriColorShader*)(cs.get()));
            drawConvexPolygon(verts, 3, GPaint(tcs.get()));
        }
    }

    /// @brief Bilinear interpolation of payload for each sub-quad after the division
    /// @param payload the payload to interpolate
    /// @param level LoD of subdivisions
    /// @return 2-D array of interpolated payload after the division
    //  * We divide the original quad into (level + 1) * (level + 1) sub-quads.
    //  *      0---1 (s)
    //  *      |   | 
    //  *      l---r  For each t, we get a left and right point (p0, p1); We then
    //  *      |   |  interpolate the points between (p0, p1).
    //  *      |   |
    //  *      3---2
    //  *     (t)
    GPoint** bilinearInterpolatePayload(const GPoint payload[4], int level) {
        if (payload == nullptr) return nullptr;

        GPoint** interpolations = new GPoint*[level + 2];
        for (int t = 0; t < level + 2; t ++) {
            interpolations[t] = new GPoint[level + 2];
            GPoint p0 = payload[0] + (payload[3] - payload[0]) * ((float)t / (level + 1));
            GPoint p1 = payload[1] + (payload[2] - payload[1]) * ((float)t / (level + 1));
            for (int s = 0; s < level + 2; s ++) {
                interpolations[t][s] = p0 + (p1 - p0) * ((float)s / (level + 1));
            }
        }
        return interpolations;
    }

    GColor** bilinearInterpolatePayload(const GColor payload[4], int level) {
        if (payload == nullptr) return nullptr;

        GColor** interpolations = new GColor*[level + 2];
        for (int t = 0; t < level + 2; t ++) {
            interpolations[t] = new GColor[level + 2];
            GColor p0 = payload[0] + (payload[3] - payload[0]) * ((float)t / (level + 1));
            GColor p1 = payload[1] + (payload[2] - payload[1]) * ((float)t / (level + 1));
            for (int s = 0; s < level + 2; s ++) {
                interpolations[t][s] = p0 + (p1 - p0) * ((float)s / (level + 1));
            }
        }
        return interpolations;
    }

    /**
     * @brief Determine if the scan line is the last scan line that will 
     * touch the given edge.
     */
    bool edgeHasExpired(GEdge edge, int scan, vector<GEdge>edges) {
        if (!(scan <= edge.bot && scan >= edge.top)){
            
        }
        return scan == edge.bot - 1;
    }

    /**
     * @brief Blend pixels between left and right index on row y with the paint.
     * 
     * @param left [Inclusive] Left-most index of the pixel included by the shape.
     * @param right [Inclusive] Right-most index of the pixel included by the shape.
     * @param row y value of the row.
     * @param paint Source paint.
     * @param hasShader whether paint is using a shader or not
     */
    void fillRow(int left, int right, int row, const GPaint& paint, bool hasShader) {
        // assert(count >= 0);
        //!! opt: default parameter, pass in shaderptr
        if (left < 0) left = 0;
        if (right < 0) right = 0;
        if (left >= fDevice.width()) left = fDevice.width() - 1;
        if (right >= fDevice.width()) right = fDevice.width() - 1;
        rowBlender rb = blenders.getBlender(paint.getBlendMode());
        int count = right - left + 1;
        
        if (!hasShader) { 
            // Shader is not used 
            GPixel srcPixel = Blenders::prepSrcPixel(paint.getColor());
            rb(left, count, &srcPixel, false, fDevice.getAddr(left, row));
        } else {
            // Shader is used
            GShader* shaderptr = paint.getShader();
            if (shaderptr->isOpaque()) {
                // Opague color will overwrite the original color completely
                shaderptr->shadeRow(left, row, count, fDevice.getAddr(left, row));
            } else {
                assert(count >= 0);
                GPixel* srcPixels = new GPixel[count];
                shaderptr->shadeRow(left, row, count, srcPixels);
                rb(left, count, srcPixels, true, fDevice.getAddr(left, row));
                delete[] srcPixels; // prevent memory leaks
            }
        }
    }

    /**
     * @brief Prepare the GEdge data structure from two GPoint points.
     * Ensure p1.Y < p2.Y.
     */
    void prepGEdge(GPoint p1, GPoint p2, int orientation, vector<GEdge>& edges) {
        // printf("(%f, %f) -> (%f, %f) \n", p1.fX, p1.fY, p2.fX, p2.fY); 
        assert(p1.fY <= p2.fY);
        int top = GRoundToInt(p1.fY);
        int bot = GRoundToInt(p2.fY);
        if (top == bot) return; // remove "narrow" edges

        float m = (p1.fX - p2.fX) / (p1.fY - p2.fY);
        float b = p1.fX - m * p1.fY;
        // printf("m = %f, b = %f \n", m, b);
        edges.push_back(GEdge({orientation, top, bot, m, b}));        
    }

    /**
     * @brief Clip edge and add the clipped edge into the list of all edges.
     */
    void clip(GPoint p1, GPoint p2, vector<GEdge>& edges) {
        int orientation;
        p1.fY < p2.fY ? orientation = -1 : orientation = 1;
        
        // Ensure p1 has smaller y value
        if (p2.fY < p1.fY) {
            GPoint temp = p1;
            p1 = p2;
            p2 = temp;
        }

        /* Vertical Clipping */
        // for p1
        if (p1.fY < 0) {
            if (p2.fY < 0) return; // reject top
            float ratio = (- p1.fY) / (p2.fY - p1.fY);
            float base = p2.fX - p1.fX;
            assert(ratio > 0 && ratio <= 1);
            p1.fY = 0;
            p1.fX += base * ratio;
        } 

        // for p2
        int maxHeight = fDevice.height();
        if (p2.fY > maxHeight) {
            if (p1.fY > maxHeight) return; // reject bot
            float ratio = (p2.fY - maxHeight) / (p2.fY - p1.fY);
            float base = p1.fX - p2.fX;
            assert(ratio > 0 && ratio <= 1);
            p2.fY = maxHeight;
            p2.fX += base * ratio;
        }

        /* Horizontal Clipping */
        // when prepGEdge(p3, p4, edges) is called, p3 and p4 should represent the
        // the proper newly-created vertex if clipping occurs.
        GPoint p3, p4; 
        if (p1.fX < p2.fX) { p3 = p1; p4 = p2; }
        else { p3 = p2; p4 = p1; }
        
        // left clipping
        if (p1.fX < 0) {
            if (p2.fX < 0) {
                GPoint proj_p1 = GPoint({0, p1.fY});
                GPoint proj_p2 = GPoint({0, p2.fY});
                prepGEdge(proj_p1, proj_p2, orientation, edges);
                return;
            }
            float ratio = (- p1.fX) / (p2.fX - p1.fX);
            float base = p2.fY - p1.fY; //!! extractable
            assert(ratio > 0 && ratio <= 1);
            p1.fX = 0;
            float p3Y = p1.fY + ratio * base;
            p3 = GPoint({0, p3Y});
            prepGEdge(p1, p3, orientation, edges);
        } 
        if (p2.fX < 0) {
            float ratio = (- p2.fX) / (p1.fX - p2.fX);
            float base = p2.fY - p1.fY;
            assert(ratio > 0 && ratio <= 1);
            p2.fX = 0;
            float p3Y = p2.fY - ratio * base;
            p3 = GPoint({0, p3Y});
            prepGEdge(p3, p2, orientation, edges);
        }

        int maxWidth = fDevice.width();
        // right clipping
        if (p1.fX > maxWidth) {
            if (p2.fX > maxWidth) {
                GPoint proj_p1 = {maxWidth, p1.fY};
                GPoint proj_p2 = {maxWidth, p2.fY};
                prepGEdge(proj_p1, proj_p2, orientation, edges);
                return;
            }
            float ratio = (p1.fX - maxWidth) / (p1.fX - p2.fX);
            float base = p2.fY - p1.fY;
            assert(ratio > 0 && ratio <= 1);
            p1.fX = maxWidth;
            float p4Y = p1.fY + ratio * base;
            p4 = {(float)maxWidth, p4Y};
            prepGEdge(p1, p4, orientation, edges);
        }
        if (p2.fX > maxWidth) {
            float ratio = (p2.fX - maxWidth) / (p2.fX - p1.fX);
            float base = p2.fY - p1.fY;
            assert(ratio > 0 && ratio <= 1);
            p2.fX = maxWidth;
            float p4Y = p2.fY - ratio * base;
            p4 = {(float)maxWidth, p4Y};
            prepGEdge(p4, p2, orientation, edges);
        }
        p3.fY < p4.fY ? prepGEdge(p3, p4, orientation, edges) : prepGEdge(p4, p3, orientation, edges);
    }

    void clipQuadBezierCurve(const GPoint points[], vector<GEdge>& edges) {
        QuadBezierCurve qbc;
        qbc.setControlPoints(points);
        GPoint E = (points[0] - 2 * points[1] + points[2]) * 0.25;
        assert(tolerance - 0.25 < 0.001);
        // Hard-coded calculation results for how many curves created through subdivisions
        // is required for the approximation to satisfy the tolerance requirement
        int num_segs = (int)ceil(sqrt(E.length() * (1 / tolerance)));

        float dt = 1.0f / num_segs;
        float t = dt;
        GPoint p0 = points[0];
        GPoint p1;
        for (int i = 1; i < num_segs; ++i) {
            p1 = qbc.eval(t);
            clip(p0, p1, edges);
            t += dt;
            p0 = p1;
        }
        p1 = points[2];
        clip(p0, p1, edges);
    }

    void clipCubicBezierCurve(const GPoint points[], vector<GEdge>& edges) {
        CubicBezierCurve cbc;
        cbc.setControlPoints(points);
        GPoint E0 = points[0] + 2 * points[1] + points[2];
        GPoint E1 = points[1] + 2 * points[2] + points[3];
        GPoint E;
        E.fX = max(abs(E0.fX), abs(E1.fX));
        E.fY = max(abs(E0.fY), abs(E1.fY));
        assert(tolerance - 0.25 < 0.001);
        int num_segs = (int)ceil(sqrt((3 * E.length()) / (4 * tolerance)));

        float dt = 1.0f / num_segs;
        float t = dt;
        GPoint p0 = points[0];
        GPoint p1;
        for (int i = 1; i < num_segs; ++i) {
            p1 = cbc.eval(t);
            clip(p0, p1, edges);
            t += dt;
            p0 = p1;
        }
        p1 = points[3];
        clip(p0, p1, edges);
    }

    /**
     * @brief Assemble points into edges by clipping all edges.
     */
    vector<GEdge> assembleEdges(const GPoint points[], int count) {
        vector<GEdge> edges;
        for (int i = 0; i < count - 1; i ++) {
            clip(points[i], points[i + 1], edges);
        }
        clip(points[count - 1], points[0], edges);
        return edges;
    }

    /**
     * @brief Assemble a path into edges by clipping all edges.
     */
    vector<GEdge> assembleEdges(const GPath& path) {
        vector<GEdge> edges;
        GPoint pts[GPath::kMaxNextPoints];
        GPath::Edger iter(path);
        GPath::Verb v;
        while ((v = iter.next(pts)) != GPath::kDone) {
            switch (v) {
                case GPath::kLine:
                    clip(pts[0], pts[1], edges);
                    break;
                case GPath::kQuad:
                    clipQuadBezierCurve(pts, edges);
                    break;
                case GPath::kCubic:
                    clipCubicBezierCurve(pts, edges);
                    break;
                default:
                    break;
            }
        }
        // printf("assembleEdges: edges.size(): %ld", edges.size());
        return edges;
    }

    /**
     * @brief Sort the [0,...,rightIdx] edges in the given edges according to
     * their intersection with the current scan line.
     * 
     * @param edges ...
     * @param currScanY the Y value of the current scan line (without _ 0.5)
     * @param lastEdgeIdx index of the last active edge that the scan line will hit
     */
    void resortByCurrX(vector<GEdge>& edges, int currScanY, int lastEdgeIdx) {
        std::sort(edges.begin(), edges.begin() + lastEdgeIdx, [=](GEdge e1, GEdge e2){
            float x1 = e1.m * ((float)currScanY + 0.5) + e1.b;
            float x2 = e2.m * ((float)currScanY + 0.5) + e2.b;
            return x1 < x2;
        });
    }

    bool isActive(GEdge edge, int currScanY) {
        if (currScanY > edge.bot) {
            // printf("here");
        }
        // assert(currScanY <= edge.bot);
        return currScanY >= edge.top;
    }
    

    
    ///////////////////////////////////////////////////////////////////////////////////////////////

};

std::unique_ptr<GCanvas> GCreateCanvas(const GBitmap& device) {
    return std::unique_ptr<GCanvas>(new Canvas(device));
}

std::string GDrawSomething(GCanvas* canvas, GISize dim) {
    GColor color1 = {0.5, 0.2, 0.4, 1};    
    GPaint paint1 = GPaint(color1);
    paint1.setBlendMode(GBlendMode::kDstATop);

    canvas->drawPaint(paint1);

    return "mySomething";
}