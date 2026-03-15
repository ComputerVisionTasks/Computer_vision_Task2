/*
 * contour_analysis.cpp — Freeman 8-direction Chain Code + Contour Metrics
 * Boundary tracing, chain code, perimeter (via chain code), area (shoelace).
 */

#include "contour_analysis.h"

ContourAnalysis analyze_contour(const GrayImage& edges) {
    ContourAnalysis result;
    int h = edges.h, w = edges.w;

    // Find start point
    int sx = -1, sy = -1;
    for (int y = 0; y < h && sx < 0; y++)
        for (int x = 0; x < w && sx < 0; x++)
            if (edges.at(x, y) >= 0.5f) { sx = x; sy = y; }
    if (sx < 0) return result;

    // Direction vectors: E, SE, S, SW, W, NW, N, NE
    const int ddx[] = {1, 1, 0, -1, -1, -1, 0, 1};
    const int ddy[] = {0, 1, 1, 1, 0, -1, -1, -1};

    auto& bnd = result.boundary;
    bnd.push_back({sx, sy});
    int cx = sx, cy = sy, prevDir = 7;

    for (int step = 0; step < w * h; step++) {
        int startDir = (prevDir + 5) % 8;
        bool found = false;
        for (int i = 0; i < 8; i++) {
            int d = (startDir + i) % 8;
            int nx = cx + ddx[d], ny = cy + ddy[d];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h && edges.at(nx, ny) >= 0.5f) {
                cx = nx; cy = ny; prevDir = d; found = true;
                result.chainCode.push_back(d);
                break;
            }
        }
        if (!found) break;
        if (bnd.size() > 3 && abs(cx - sx) < 2 && abs(cy - sy) < 2) break;
        bnd.push_back({cx, cy});
    }

    result.numPoints = (int)bnd.size();
    result.isClosed = (bnd.size() > 3 &&
                       abs(bnd.front().first - bnd.back().first) < 2 &&
                       abs(bnd.front().second - bnd.back().second) < 2);

    // Perimeter from chain code
    for (int c : result.chainCode)
        result.perimeter += (c % 2 == 0) ? 1.0f : 1.41421356f;

    // Area via shoelace formula
    float a = 0;
    int n = (int)bnd.size();
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += bnd[i].first * bnd[j].second - bnd[j].first * bnd[i].second;
    }
    result.area = fabsf(a) / 2.0f;
    return result;
}
