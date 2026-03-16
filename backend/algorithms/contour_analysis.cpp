// contour_analysis.cpp — full corrected file

#include "contour_analysis.h"
#include <cmath>
#include <algorithm>
#include <climits>

ContourAnalysis analyze_contour(const GrayImage& edges) {
    ContourAnalysis result;
    int h = edges.h, w = edges.w;

    int sx = -1, sy = -1;
    for (int y = 0; y < h && sx < 0; y++)
        for (int x = 0; x < w && sx < 0; x++)
            if (edges.at(x, y) >= 0.5f) { sx = x; sy = y; }
    if (sx < 0) return result;

    // Freeman 8-direction: E SE S SW W NW N NE
    const int ddx[] = { 1,  1,  0, -1, -1, -1,  0,  1 };
    const int ddy[] = { 0,  1,  1,  1,  0, -1, -1, -1 };

    auto& bnd = result.boundary;
    bnd.push_back({sx, sy});
    int cx = sx, cy = sy, prevDir = 7;
    bool movedAway = false;

    for (int step = 0; step < w * h; step++) {
        // Search CCW starting from reverse of incoming direction
        int startDir = (prevDir + 5) % 8;
        bool found = false;

        for (int i = 0; i < 8; i++) {
            int d = (startDir + i) % 8;
            int nx = cx + ddx[d], ny = cy + ddy[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (edges.at(nx, ny) < 0.5f) continue;

            cx = nx; cy = ny; prevDir = d;
            result.chainCode.push_back(d);
            found = true;
            break;
        }

        if (!found) break;

        if (!movedAway) {
            int distSq = (cx-sx)*(cx-sx) + (cy-sy)*(cy-sy);
            if (distSq >= 9) movedAway = true;  // 3 pixels away
        }

        if (movedAway && (int)bnd.size() > 3) {
            if (std::abs(cx - sx) < 2 && std::abs(cy - sy) < 2) break;
        }

        bnd.push_back({cx, cy});
        if ((int)bnd.size() >= w * h) break;
    }

    result.numPoints = (int)bnd.size();
    result.isClosed = (bnd.size() > 3 &&
                       std::abs(bnd.front().first  - bnd.back().first)  < 2 &&
                       std::abs(bnd.front().second - bnd.back().second) < 2);

    for (int c : result.chainCode)
        result.perimeter += (c % 2 == 0) ? 1.0f : 1.41421356f;

    float a = 0;
    int n = (int)bnd.size();
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += bnd[i].first * bnd[j].second - bnd[j].first * bnd[i].second;
    }
    result.area = std::fabsf(a) / 2.0f;
    return result;
}

// ─────────────────────────────────────────────────────────────
// FIX 1: overlay uses the actual base image dimensions, so the
//         colored contour lines are pixel-accurate on the source image
// ─────────────────────────────────────────────────────────────
// The TWO outputs explained:
//
// render_freeman_overlay  — contour drawn ON TOP of the original image
//                           (colored segments overlaid on the photo/edges)
//
// render_freeman_code_image — contour drawn on a BLACK canvas alone,
//                             so direction colors are clearly visible
//                             without the background image competing
//
// Both draw the SAME thing: the contour path, segment by segment,
// colored by Freeman direction code. The difference is only the background.

RGBImage render_freeman_overlay(const RGBImage& base, const ContourAnalysis& ca) {
    RGBImage out = base;
    if (ca.boundary.size() < 2) return out;

    const unsigned char COLORS[8][3] = {
        {255,  64,  64},   // 0 E   red
        {255, 165,   0},   // 1 SE  orange
        { 64, 220,  64},   // 2 S   green
        {  0, 210, 180},   // 3 SW  teal
        { 64, 128, 255},   // 4 W   blue
        {180,  64, 255},   // 5 NW  purple
        {255,  64, 220},   // 6 N   pink
        {255, 220,  64},   // 7 NE  yellow
    };

    // FIX: draw each boundary POINT directly — don't try to connect
    // boundary[i] to boundary[i+1] with a line, because the chain code
    // already guarantees each step is exactly 1 or sqrt(2) pixels.
    // Connecting with Bresenham introduces phantom pixels at diagonals.
    for (size_t i = 0; i < ca.boundary.size(); i++) {
        int x = ca.boundary[i].first;
        int y = ca.boundary[i].second;
        int dir = (i < ca.chainCode.size()) ? ca.chainCode[i] : 0;
        const unsigned char* c = COLORS[dir & 7];

        // Draw larger direction dots (radius 2) for clearer direction visibility.
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                if (dx * dx + dy * dy <= 4)
                    out.set(x + dx, y + dy, c[0], c[1], c[2]);
    }
    return out;
}

RGBImage render_freeman_code_image(const ContourAnalysis& ca, int w, int h) {
    RGBImage out(w, h);  // starts as all black — correct background

    if (ca.boundary.size() < 2) return out;

    const unsigned char COLORS[8][3] = {
        {255,  64,  64},
        {255, 165,   0},
        { 64, 220,  64},
        {  0, 210, 180},
        { 64, 128, 255},
        {180,  64, 255},
        {255,  64, 220},
        {255, 220,  64},
    };

    // FIX: the boundary coords come from analyze_contour which traced
    // the EDGE image — but the canvas passed in (w, h) might be the
    // ORIGINAL image size, not the edge image size.
    // We need to know what size the edge image was so we can scale.
    // 
    // Since we don't have that here, find the bounding box of the
    // boundary points and scale to fill the output canvas.
    int minX = INT_MAX, minY = INT_MAX, maxX = 0, maxY = 0;
    for (auto& pt : ca.boundary) {
        minX = std::min(minX, pt.first);
        minY = std::min(minY, pt.second);
        maxX = std::max(maxX, pt.first);
        maxY = std::max(maxY, pt.second);
    }

    int bndW = maxX - minX + 1;
    int bndH = maxY - minY + 1;

    // Scale factor: fit inside canvas with 10px margin
    float margin = 10.0f;
    float scaleX = (bndW > 0) ? (w - 2 * margin) / (float)bndW : 1.0f;
    float scaleY = (bndH > 0) ? (h - 2 * margin) / (float)bndH : 1.0f;
    float scale  = std::min(scaleX, scaleY);  // uniform scale, no distortion

    // Center the contour in the output image
    float offsetX = (w - bndW * scale) / 2.0f;
    float offsetY = (h - bndH * scale) / 2.0f;

    // Draw each boundary point, scaled and centered
    for (size_t i = 0; i < ca.boundary.size(); i++) {
        float fx = (ca.boundary[i].first  - minX) * scale + offsetX;
        float fy = (ca.boundary[i].second - minY) * scale + offsetY;
        int px = (int)roundf(fx);
        int py = (int)roundf(fy);

        int dir = (i < ca.chainCode.size()) ? ca.chainCode[i] : 0;
        const unsigned char* c = COLORS[dir & 7];

        // Larger dots for direction color-map readability.
        int r = std::max(2, (int)(scale * 0.9f));
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (dx*dx + dy*dy <= r*r)
                    out.set(px + dx, py + dy, c[0], c[1], c[2]);
    }
    return out;
}