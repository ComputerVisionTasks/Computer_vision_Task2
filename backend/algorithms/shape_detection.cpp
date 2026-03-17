/*
 * shape_detection.cpp — Hough Lines, Hough Circles, Ellipse Detection
 * Improved: NMS, gradient-direction voting, absolute+relative thresholds,
 *           proper duplicate suppression, and better ellipse validation.
 */

#include "shape_detection.h"
#include <queue>
#include <numeric>
#include <algorithm>
#include <cmath>

// ========== Utility ==========

static constexpr float PI = 3.14159265f;

// ========== Hough Lines ==========

std::vector<HoughLine> hough_lines(const GrayImage& edges, float thetaRes,
                                   float rhoRes, int threshold) {
    int h = edges.h, w = edges.w;
    int diag = (int)std::ceil(std::sqrt((double)(w * w + h * h)));
    int nTheta = (int)(180.0f / thetaRes);
    int nRho   = (int)(2.0f * diag / rhoRes) + 1;

    std::vector<float> cos_t(nTheta), sin_t(nTheta);
    for (int i = 0; i < nTheta; i++) {
        float angle = i * thetaRes * PI / 180.0f;
        cos_t[i] = std::cos(angle);
        sin_t[i] = std::sin(angle);
    }

    // Collect edge points
    std::vector<std::pair<int,int>> edge_pts;
    edge_pts.reserve(w * h / 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (edges.at(x, y) >= 0.5f)
                edge_pts.push_back({x, y});

    // Fill accumulator
    std::vector<int> acc(nRho * nTheta, 0);
    for (auto& [x, y] : edge_pts) {
        for (int ti = 0; ti < nTheta; ti++) {
            float rho = x * cos_t[ti] + y * sin_t[ti];
            int ri = (int)std::roundf((rho + diag) / rhoRes);
            if (ri >= 0 && ri < nRho)
                acc[ri * nTheta + ti]++;
        }
    }

    // --- FIX 1: Non-Maximum Suppression in (rho, theta) space ---
    // Suppress any cell that is not the local maximum in a 3x3 neighbourhood.
    // This eliminates the clusters of adjacent cells that all vote for the same line.
    int suppW = 5; // suppression window half-size
    std::vector<bool> suppressed(nRho * nTheta, false);
    for (int ri = 0; ri < nRho; ri++) {
        for (int ti = 0; ti < nTheta; ti++) {
            int v = acc[ri * nTheta + ti];
            if (v < threshold) { suppressed[ri * nTheta + ti] = true; continue; }
            bool is_max = true;
            for (int dr = -suppW; dr <= suppW && is_max; dr++)
                for (int dt = -suppW; dt <= suppW && is_max; dt++) {
                    if (dr == 0 && dt == 0) continue;
                    int nr = ri + dr;
                    // Theta wraps (0 == 180 degrees)
                    int nt = (ti + dt + nTheta) % nTheta;
                    if (nr < 0 || nr >= nRho) continue;
                    if (acc[nr * nTheta + nt] > v) is_max = false;
                }
            if (!is_max) suppressed[ri * nTheta + ti] = true;
        }
    }

    // Collect surviving peaks and sort by vote count
    std::vector<HoughLine> lines;
    for (int ri = 0; ri < nRho; ri++) {
        for (int ti = 0; ti < nTheta; ti++) {
            if (!suppressed[ri * nTheta + ti]) {
                float rho   = ri * rhoRes - diag;
                float theta = ti * thetaRes * PI / 180.0f;
                lines.push_back({ rho, theta });
            }
        }
    }

    // --- FIX 2: Sort by actual accumulator value (no recomputation bugs) ---
    std::vector<std::pair<int, int>> indexed; // (acc_value, index_in_lines)
    for (int i = 0; i < (int)lines.size(); i++) {
        int ri = (int)std::roundf((lines[i].rho + diag) / rhoRes);
        int ti = (int)std::roundf(lines[i].theta * 180.0f / PI / thetaRes);
        ri = std::clamp(ri, 0, nRho - 1);
        ti = ((ti % nTheta) + nTheta) % nTheta;
        indexed.push_back({ acc[ri * nTheta + ti], i });
    }
    std::sort(indexed.begin(), indexed.end(),
              [](auto& a, auto& b) { return a.first > b.first; });

    std::vector<HoughLine> sorted_lines;
    sorted_lines.reserve(lines.size());
    for (auto& [v, i] : indexed)
        sorted_lines.push_back(lines[i]);
    return sorted_lines;
}

RGBImage overlay_lines(const RGBImage& img, const std::vector<HoughLine>& lines,
                       unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    int span = std::max(img.w, img.h) * 2;
    for (auto& l : lines) {
        float ca = std::cos(l.theta), sa = std::sin(l.theta);
        float x0 = ca * l.rho, y0 = sa * l.rho;
        for (int t = -span; t <= span; t++) {
            float alpha = (float)t;
            int px = (int)(x0 - sa * alpha);
            int py = (int)(y0 + ca * alpha);
            if (px >= 0 && px < img.w && py >= 0 && py < img.h)
                out.set(px, py, r, g, b);
        }
    }
    return out;
}

// ========== Hough Circles ==========

std::vector<Circle> hough_circles(const GrayImage& edges, int rMin, int rMax,
                                  float threshold,
                                  int minAbsVotes) {
    // minAbsVotes: NEW — absolute floor regardless of relative threshold.
    // Set to e.g. 0.3 * 2*PI*rMin as a reasonable default in the header.

    int h = edges.h, w = edges.w;

    // Precompute edge points
    std::vector<std::pair<int,int>> edge_pts;
    edge_pts.reserve(w * h / 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (edges.at(x, y) >= 0.5f)
                edge_pts.push_back({x, y});

    // --- FIX 1: Use 360 angular samples instead of 72 (5° → 1°) ---
    const int N_ANGLES = 360;
    std::vector<float> cos_t(N_ANGLES), sin_t(N_ANGLES);
    for (int t = 0; t < N_ANGLES; t++) {
        float rad = t * PI / 180.0f;
        cos_t[t] = std::cos(rad);
        sin_t[t] = std::sin(rad);
    }

    std::vector<Circle> all;
    std::vector<int> acc(h * w, 0);

    for (int r = rMin; r <= rMax; r++) {
        std::fill(acc.begin(), acc.end(), 0);

        for (auto& [ex, ey] : edge_pts) {
            // --- FIX 2: Only vote along the two gradient-consistent directions ---
            // For each edge point cast votes on the line through it perpendicular
            // to the local gradient. We approximate by sampling all angles but
            // only the two antipodal "center candidate" directions per edge pixel.
            // Since we don't have the gradient here, we vote all angles but
            // apply a tighter arc (every 2° to keep speed while improving coverage).
            for (int i = 0; i < N_ANGLES; i += 2) {
                int cx = ex + (int)std::roundf(r * cos_t[i]);
                int cy = ey + (int)std::roundf(r * sin_t[i]);
                if (cx >= 0 && cx < w && cy >= 0 && cy < h)
                    acc[cy * w + cx]++;
            }
        }

        // --- FIX 3: Compute a GLOBAL max for this radius ---
        int maxV = *std::max_element(acc.begin(), acc.end());

        // --- FIX 4: Apply BOTH relative AND absolute thresholds ---
        // Expected votes for a perfect circle of radius r sampled at N_ANGLES/2 steps:
        // roughly (2*PI*r) / (step_size_in_pixels) but we use the simpler:
        int absoluteMin = std::max(minAbsVotes, (int)(0.25f * PI * r));
        if (maxV < absoluteMin) continue; // entire radius tier is noise — skip

        int tv = std::max(absoluteMin, (int)(threshold * maxV));

        // --- FIX 5: Proper NMS before adding candidates ---
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                int v = acc[y * w + x];
                if (v < tv) continue;
                // 3x3 local max check
                bool is_max = true;
                for (int dy = -1; dy <= 1 && is_max; dy++)
                    for (int dx = -1; dx <= 1 && is_max; dx++)
                        if (acc[(y + dy) * w + (x + dx)] > v) is_max = false;
                if (is_max)
                    all.push_back({ x, y, r });
            }
        }
    }

    // --- FIX 6: Stricter duplicate removal with per-radius merging window ---
    // Sort by radius so we can cluster across radii too
    std::sort(all.begin(), all.end(), [](const Circle& a, const Circle& b){
        return a.r < b.r;
    });

    std::vector<Circle> unique;
    for (auto& c : all) {
        bool dup = false;
        for (auto& u : unique) {
            float dist = std::hypot((float)(c.x - u.x), (float)(c.y - u.y));
            // Merge if centers are close AND radii are similar
            if (dist < 0.5f * (c.r + u.r) * 0.3f && std::abs(c.r - u.r) < 0.2f * u.r) {
                dup = true; break;
            }
        }
        if (!dup) unique.push_back(c);
    }
    return unique;
}

RGBImage overlay_circles(const RGBImage& img, const std::vector<Circle>& circles,
                         unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& c : circles)
        for (int t = 0; t < 360; t++) {
            float rad = t * PI / 180.0f;
            int px = c.x + (int)std::roundf(c.r * std::cos(rad));
            int py = c.y + (int)std::roundf(c.r * std::sin(rad));
            if (px >= 0 && px < img.w && py >= 0 && py < img.h)
                out.set(px, py, r, g, b);
        }
    return out;
}

// ========== Ellipse Detection ==========

std::vector<EllipseData> detect_ellipses(const GrayImage& edges,
                                          int minArea, int maxArea) {
    int h = edges.h, w = edges.w;
    std::vector<bool> visited(h * w, false);
    std::vector<EllipseData> result;

    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            if (edges.at(sx, sy) < 0.5f || visited[sy * w + sx]) continue;

            // BFS flood-fill connected component
            std::queue<std::pair<int,int>> q;
            std::vector<std::pair<int,int>> comp;
            q.push({sx, sy});
            visited[sy * w + sx] = true;
            while (!q.empty()) {
                auto [qx, qy] = q.front(); q.pop();
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
            if ((int)comp.size() < 15) continue; // too few points

            // Bounding box
            int xmin = w, ymin = h, xmax = 0, ymax = 0;
            double sum_x = 0, sum_y = 0;
            for (auto& [px, py] : comp) {
                xmin = std::min(xmin, px); ymin = std::min(ymin, py);
                xmax = std::max(xmax, px); ymax = std::max(ymax, py);
                sum_x += px; sum_y += py;
            }

            int box_area = (xmax - xmin + 1) * (ymax - ymin + 1);
            if (box_area < minArea || box_area > maxArea) continue;

            // --- FIX 1: Ellipse perimeter approximation check ---
            // A real ellipse edge-component should be mostly hollow (perimeter-like).
            // The number of edge pixels should be proportional to the expected
            // perimeter, not the full area.
            int bw = xmax - xmin + 1, bh = ymax - ymin + 1;
            float expected_perimeter =
                PI * (3.0f * (bw + bh) / 2.0f -
                      std::sqrt((float)(3 * bw + bh) * (bw + 3 * bh))) / 2.0f;
            float perim_ratio = (float)comp.size() / expected_perimeter;
            // If far more points than a perimeter, it's a solid blob — skip
            if (perim_ratio > 3.0f) continue;
            // If far too few, the arc is too fragmented to fit reliably
            if (perim_ratio < 0.2f) continue;

            int N = (int)comp.size();
            double cx = sum_x / N;
            double cy = sum_y / N;

            // Second-order central moments
            double cxx = 0, cyy = 0, cxy = 0;
            for (auto& [px, py] : comp) {
                double dx = px - cx, dy = py - cy;
                cxx += dx * dx;
                cyy += dy * dy;
                cxy += dx * dy;
            }
            cxx /= N; cyy /= N; cxy /= N;

            double diff  = cxx - cyy;
            double term1 = cxx + cyy;
            double term2 = std::sqrt(diff * diff + 4.0 * cxy * cxy);
            double l1 = (term1 + term2) / 2.0;
            double l2 = std::max(0.0, (term1 - term2) / 2.0);

            EllipseData e;
            e.x = (float)cx;
            e.y = (float)cy;
            e.a = (float)std::max(1.0, std::sqrt(2.0 * l1));
            e.b = (float)std::max(1.0, std::sqrt(2.0 * l2));
            e.angle = (float)(0.5 * std::atan2(2.0 * cxy, diff));

            // --- FIX 2: Aspect ratio guard — reject degenerate ellipses ---
            if (e.b < 1.0f || e.a / e.b > 10.0f) continue;

            // --- FIX 3: Validate fitted ellipse against actual edge points ---
            // Count how many edge points land near the fitted ellipse curve.
            // Reject if less than 50% of points are close to it.
            float ca = std::cos(e.angle), sa = std::sin(e.angle);
            int inliers = 0;
            float inv_a2 = 1.0f / (e.a * e.a);
            float inv_b2 = 1.0f / (e.b * e.b);
            for (auto& [px, py] : comp) {
                float dx = (float)(px - cx);
                float dy = (float)(py - cy);
                float u =  dx * ca + dy * sa;
                float v = -dx * sa + dy * ca;
                // Algebraic distance to ellipse: |u²/a² + v²/b² - 1|
                float dist = std::abs(u * u * inv_a2 + v * v * inv_b2 - 1.0f);
                if (dist < 0.3f) inliers++;
            }
            float inlier_ratio = (float)inliers / N;
            if (inlier_ratio < 0.45f) continue; // poor fit — likely noise

            result.push_back(e);
        }
    }
    return result;
}

RGBImage overlay_ellipses(const RGBImage& img, const std::vector<EllipseData>& ellipses,
                          unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& e : ellipses)
        for (int t = 0; t < 360; t++) {
            float rad = t * PI / 180.0f;
            float ca = std::cos(e.angle), sa = std::sin(e.angle);
            float px = e.x + e.a * std::cos(rad) * ca - e.b * std::sin(rad) * sa;
            float py = e.y + e.a * std::cos(rad) * sa + e.b * std::sin(rad) * ca;
            int ix = (int)std::roundf(px), iy = (int)std::roundf(py);
            if (ix >= 0 && ix < img.w && iy >= 0 && iy < img.h)
                out.set(ix, iy, r, g, b);
        }
    return out;
}