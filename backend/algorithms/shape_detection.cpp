/*
 * shape_detection.cpp — Hough Lines, Hough Circles, Ellipse Detection
 */

#include "shape_detection.h"
#include <queue>

// ========== Hough Lines ==========

std::vector<HoughLine> hough_lines(const GrayImage& edges, float thetaRes,
                                   float rhoRes, int threshold) {
    int h = edges.h, w = edges.w;
    int diag = (int)ceil(sqrt((double)(w*w + h*h)));
    int nTheta = (int)(180.0f / thetaRes);
    int nRho = (int)(2 * diag / rhoRes) + 1;

    std::vector<float> thetas(nTheta);
    for (int i = 0; i < nTheta; i++) thetas[i] = i * thetaRes * 3.14159265f / 180.0f;

    std::vector<int> acc(nRho * nTheta, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (edges.at(x, y) < 0.5f) continue;
            for (int ti = 0; ti < nTheta; ti++) {
                float rho = x * cosf(thetas[ti]) + y * sinf(thetas[ti]);
                int ri = (int)roundf((rho + diag) / rhoRes);
                if (ri >= 0 && ri < nRho) acc[ri * nTheta + ti]++;
            }
        }

    std::vector<HoughLine> lines;
    for (int ri = 0; ri < nRho; ri++)
        for (int ti = 0; ti < nTheta; ti++)
            if (acc[ri * nTheta + ti] >= threshold)
                lines.push_back({ ri * rhoRes - diag, thetas[ti] });

    std::sort(lines.begin(), lines.end(), [&](const HoughLine& a, const HoughLine& b) {
        int ai = (int)roundf((a.rho + diag) / rhoRes) * nTheta +
                 (int)roundf(a.theta * 180.0f / 3.14159265f / thetaRes);
        int bi = (int)roundf((b.rho + diag) / rhoRes) * nTheta +
                 (int)roundf(b.theta * 180.0f / 3.14159265f / thetaRes);
        ai = std::clamp(ai, 0, (int)acc.size() - 1);
        bi = std::clamp(bi, 0, (int)acc.size() - 1);
        return acc[ai] > acc[bi];
    });
    return lines;
}

RGBImage overlay_lines(const RGBImage& img, const std::vector<HoughLine>& lines,
                       unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& l : lines) {
        float a = cosf(l.theta), bv = sinf(l.theta);
        float x0 = a * l.rho, y0 = bv * l.rho;
        for (int t = 0; t < 2000; t++) {
            float alpha = (t - 1000) / 1000.0f * std::max(img.w, img.h);
            int px = (int)(x0 + alpha * (-bv));
            int py = (int)(y0 + alpha * a);
            if (px >= 0 && px < img.w && py >= 0 && py < img.h)
                out.set(px, py, r, g, b);
        }
    }
    return out;
}

// ========== Hough Circles ==========

std::vector<Circle> hough_circles(const GrayImage& edges, int rMin, int rMax,
                                  float threshold) {
    int h = edges.h, w = edges.w;
    std::vector<Circle> all;

    for (int r = rMin; r <= rMax; r++) {
        std::vector<int> acc(h * w, 0);
        for (int ey = 0; ey < h; ey++)
            for (int ex = 0; ex < w; ex++) {
                if (edges.at(ex, ey) < 0.5f) continue;
                for (int t = 0; t < 360; t += 5) {
                    float rad = t * 3.14159265f / 180.0f;
                    int cx = ex + (int)(r * cosf(rad));
                    int cy = ey + (int)(r * sinf(rad));
                    if (cx >= 0 && cx < w && cy >= 0 && cy < h)
                        acc[cy * w + cx]++;
                }
            }
        int maxV = *std::max_element(acc.begin(), acc.end());
        if (maxV > 0) {
            int tv = (int)(threshold * maxV);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if (acc[y * w + x] >= tv)
                        all.push_back({ x, y, r });
        }
    }

    // Remove duplicates
    std::vector<Circle> unique;
    for (auto& c : all) {
        bool dup = false;
        for (auto& u : unique)
            if (abs(c.x - u.x) < 5 && abs(c.y - u.y) < 5 && abs(c.r - u.r) < 5)
            { dup = true; break; }
        if (!dup) unique.push_back(c);
    }
    return unique;
}

RGBImage overlay_circles(const RGBImage& img, const std::vector<Circle>& circles,
                         unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& c : circles)
        for (int t = 0; t < 360; t++) {
            float rad = t * 3.14159265f / 180.0f;
            int px = c.x + (int)(c.r * cosf(rad));
            int py = c.y + (int)(c.r * sinf(rad));
            out.set(px, py, r, g, b);
        }
    return out;
}

// ========== Ellipse Detection ==========

std::vector<EllipseData> detect_ellipses(const GrayImage& edges, int minArea, int maxArea) {
    int h = edges.h, w = edges.w;
    std::vector<bool> visited(h * w, false);
    std::vector<EllipseData> result;

    for (int sy = 0; sy < h; sy++)
        for (int sx = 0; sx < w; sx++) {
            if (edges.at(sx, sy) < 0.5f || visited[sy * w + sx]) continue;
            std::queue<std::pair<int,int>> q;
            std::vector<std::pair<int,int>> comp;
            q.push({sx, sy});
            visited[sy * w + sx] = true;
            while (!q.empty()) {
                auto front = q.front(); q.pop();
                int qx = front.first, qy = front.second;
                comp.push_back({qx, qy});
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = qx + dx, ny = qy + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                            !visited[ny * w + nx] && edges.at(nx, ny) >= 0.5f) {
                            visited[ny * w + nx] = true;
                            q.push({nx, ny});
                        }
                    }
            }
            if (comp.size() < 6) continue;
            int xmin = w, ymin = h, xmax = 0, ymax = 0;
            for (auto& pt : comp) {
                xmin = std::min(xmin, pt.first);  ymin = std::min(ymin, pt.second);
                xmax = std::max(xmax, pt.first);  ymax = std::max(ymax, pt.second);
            }
            int area = (xmax - xmin) * (ymax - ymin);
            if (area < minArea || area > maxArea) continue;
            EllipseData e;
            e.x = (xmin + xmax) / 2.0f;
            e.y = (ymin + ymax) / 2.0f;
            e.a = (xmax - xmin) / 2.0f;
            e.b = (ymax - ymin) / 2.0f;
            e.angle = 0;
            result.push_back(e);
        }
    return result;
}

RGBImage overlay_ellipses(const RGBImage& img, const std::vector<EllipseData>& ellipses,
                          unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& e : ellipses)
        for (int t = 0; t < 360; t++) {
            float rad = t * 3.14159265f / 180.0f;
            float ca = cosf(e.angle), sa = sinf(e.angle);
            float px = e.x + e.a * cosf(rad) * ca - e.b * sinf(rad) * sa;
            float py = e.y + e.a * cosf(rad) * sa + e.b * sinf(rad) * ca;
            out.set((int)px, (int)py, r, g, b);
        }
    return out;
}
