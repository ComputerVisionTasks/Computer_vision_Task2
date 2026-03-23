/*
 * main.cpp — FromScratchCV HTTP Server
 * Enhanced with comprehensive shape detection parameters
 */
#include <iostream>
#include <ostream>
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif
#include "httplib.h"
#include "json.hpp"

#include "algorithms/image_utils.h"
#include "algorithms/edge_detection.h"
#include "algorithms/shape_detection.h"
#include "algorithms/active_contour.h"
#include "algorithms/contour_analysis.h"

#include <map>
#include <mutex>
#include <iostream>
#include <string>
#include <cmath>

using json = nlohmann::json;

// ========== Session Store ==========

struct Session {
    RGBImage original, current;
    Snake snake;
    bool snakeInited = false;
    std::vector<std::pair<int,int>> contourPts;
    std::vector<std::string> history;
};

std::mutex g_mutex;
std::map<std::string, Session> g_sessions;

// ========== Helper ==========

std::string form_val(const httplib::Request& req, const std::string& key,
                     const std::string& def = "") {
    if (req.form.has_field(key)) return req.form.get_field(key);
    return def;
}

float form_float(const httplib::Request& req, const std::string& key, float def = 0.0f) {
    std::string val = form_val(req, key, "");
    if (val.empty()) return def;
    try {
        return std::stof(val);
    } catch (...) {
        return def;
    }
}

int form_int(const httplib::Request& req, const std::string& key, int def = 0) {
    std::string val = form_val(req, key, "");
    if (val.empty()) return def;
    try {
        return std::stoi(val);
    } catch (...) {
        return def;
    }
}

bool form_bool(const httplib::Request& req, const std::string& key, bool def = false) {
    std::string val = form_val(req, key, "");
    if (val.empty()) return def;
    if (val == "1" || val == "true" || val == "TRUE" || val == "True" ||
        val == "yes" || val == "YES" || val == "on" || val == "ON") {
        return true;
    }
    if (val == "0" || val == "false" || val == "FALSE" || val == "False" ||
        val == "no" || val == "NO" || val == "off" || val == "OFF") {
        return false;
    }
    return def;
}

// ========== Main ==========

int main() {
    httplib::Server svr;

    // CORS
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "*");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // GET /
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        json j;
        j["message"] = "FromScratchCV API is running (C++)";
        j["version"] = "2.0.0";
        j["endpoints"] = {
            "/upload", "/canny",
            "/hough-lines", "/hough-circles", "/detect-ellipses",
            "/snake/init", "/snake/evolve", "/snake/reset",
            "/analyze-contour", "/reset", "/undo", "/save"
        };
        res.set_content(j.dump(), "application/json");
    });

    // ========== Upload ==========

    svr.Post("/upload", [](const httplib::Request& req, httplib::Response& res) {
        auto session_id = req.form.get_field("session_id");
        auto file = req.form.get_file("file");
        if (session_id.empty() || file.content.empty()) {
            res.status = 400;
            res.set_content(json{{"error","missing session_id or file"}}.dump(), "application/json");
            return;
        }
        RGBImage img = load_image_from_memory(
            (const unsigned char*)file.content.data(), (int)file.content.size());
        img = resize_image(img, 1024, 1024);

        std::lock_guard<std::mutex> lk(g_mutex);
        Session& s = g_sessions[session_id];
        s.original = img;
        s.current = img;
        s.snakeInited = false;
        s.history.clear();
        s.contourPts.clear();

        std::string b64 = rgb_to_base64_png(img);
        json j;
        j["success"] = true;
        j["original"] = b64;
        j["processed"] = b64;
        j["width"] = img.w;
        j["height"] = img.h;
        j["mode"] = "RGB";
        res.set_content(j.dump(), "application/json");
    });

    // ========== Edge Detection ==========

    svr.Post("/canny", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        float lo = form_float(req, "low_threshold", 0.05f);
        float hi = form_float(req, "high_threshold", 0.15f);
        float sigma = form_float(req, "sigma", 1.0f);

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, sigma, lo, hi);
        std::string edgeB64 = gray_to_base64_png(edges);

        // 50% blend overlay
        RGBImage edgeRGB = gray_to_rgb(edges);
        RGBImage overlay(s.current.w, s.current.h);
        for (int i = 0; i < s.current.w * s.current.h * 3; i++)
            overlay.data[i] = (unsigned char)((s.current.data[i] + edgeRGB.data[i]) / 2);
        std::string overlayB64 = rgb_to_base64_png(overlay);

        s.history.push_back("canny");
        json j;
        j["success"] = true;
        j["processed"] = edgeB64;
        j["overlay"] = overlayB64;
        res.set_content(j.dump(), "application/json");
    });

    // ========== Enhanced Shape Detection ==========

    // Enhanced Hough Lines with full parameter support
    svr.Post("/hough-lines", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int threshold = form_int(req, "threshold", 50);
        float thetaRes = form_float(req, "theta_res", 1.0f);
        float rhoRes = form_float(req, "rho_res", 1.0f);

        std::cout << "[Hough Lines] Threshold: " << threshold 
                  << ", Theta Res: " << thetaRes 
                  << ", Rho Res: " << rhoRes << std::endl;

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        
        // Use current image (which may have edges already, but we compute fresh for consistency)
        GrayImage gray = to_gray(s.current);
        // Use sigma=1.4 for shape detection: more smoothing → cleaner longer
        // edges, fewer broken fragments → better Hough accumulation.
        GrayImage edges = canny(gray, 1.4f, 0.04f, 0.12f);
        auto lines = hough_lines(edges, thetaRes, rhoRes, threshold);

        RGBImage ov = overlay_lines(s.current, lines, 255, 0, 0); // Red for lines
        json jlines = json::array();
        for (auto& l : lines) jlines.push_back({l.rho, l.theta});

        json j;
        j["success"] = true;
        j["lines"] = jlines;
        j["overlay"] = rgb_to_base64_png(ov);
        j["num_lines"] = (int)lines.size();
        
        std::cout << "[Hough Lines] Detected " << lines.size() << " lines" << std::endl;
        res.set_content(j.dump(), "application/json");
    });

    // Enhanced Hough Circles with comprehensive parameters
    svr.Post("/hough-circles", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int rMin = form_int(req, "radius_min", 10);
        int rMax = form_int(req, "radius_max", 100);
        float thr = form_float(req, "threshold", 0.55f);
        int minAbsVotes = form_int(req, "min_abs_votes", 20);
        float centerDist = form_float(req, "center_dist", 0.3f);

        std::cout << "[Hough Circles] Radius: " << rMin << "-" << rMax
                  << ", Threshold: " << thr
                  << ", Min Votes: " << minAbsVotes
                  << ", Center Dist: " << centerDist << std::endl;

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, 1.4f, 0.04f, 0.12f);
        auto circles = hough_circles(edges, rMin, rMax, thr, minAbsVotes, centerDist);

        RGBImage ov = overlay_circles(s.current, circles, 0, 255, 0); // Green for circles
        json jc = json::array();
        for (auto& c : circles) jc.push_back({c.x, c.y, c.r});

        json j;
        j["success"] = true;
        j["circles"] = jc;
        j["overlay"] = rgb_to_base64_png(ov);
        j["num_circles"] = (int)circles.size();
        
        std::cout << "[Hough Circles] Detected " << circles.size() << " circles" << std::endl;
        res.set_content(j.dump(), "application/json");
    });

    // Enhanced Ellipse Detection with comprehensive parameters
    svr.Post("/detect-ellipses", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int minArea = form_int(req, "min_area", 200);
        int maxArea = form_int(req, "max_area", 10000);
        float tolerance = form_float(req, "tolerance", 0.1f);
        float inlierRatio = form_float(req, "inlier_ratio", 0.45f);
        float minAspect = form_float(req, "min_aspect", 0.1f);

        std::cout << "[Ellipse Detection] Area: " << minArea << "-" << maxArea 
                  << ", Tolerance: " << tolerance << std::endl;

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, 0.8f, 0.01f, 0.06f);
        auto els = detect_ellipses(edges, minArea, maxArea, tolerance, inlierRatio, minAspect);

        RGBImage ov = overlay_ellipses(s.current, els, 0, 0, 255); // Blue for ellipses
        json je = json::array();
        for (auto& e : els)
            je.push_back({{"x",e.x},{"y",e.y},{"a",e.a},{"b",e.b},{"angle",e.angle}});

        json j;
        j["success"] = true;
        j["ellipses"] = je;
        j["overlay"] = rgb_to_base64_png(ov);
        j["num_ellipses"] = (int)els.size();
        
        std::cout << "[Ellipse Detection] Detected " << els.size() << " ellipses" << std::endl;
        res.set_content(j.dump(), "application/json");
    });

    // ========== Active Contour ==========

    svr.Post("/snake/init", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        auto ptsStr = req.form.get_field("points");
        float alpha = form_float(req, "alpha", 0.5f);
        float beta  = form_float(req, "beta", 0.5f);
        float gamma = form_float(req, "gamma", 1.0f);

        std::cout << "[Snake Init] Session: " << sid << ", Points JSON length: " << ptsStr.length() << std::endl;

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            std::cout << "[Snake Init] ERROR: Session not found" << std::endl;
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        
        try {
            json jp = json::parse(ptsStr);
            std::vector<std::pair<float,float>> pts;
            for (auto& p : jp) pts.push_back({p[0].get<float>(), p[1].get<float>()});
            
            std::cout << "[Snake Init] Parsed " << pts.size() << " points. Image size: " 
                      << s.current.w << "x" << s.current.h << std::endl;

            GrayImage gray = to_gray(s.current);
            s.snake.init(gray, alpha, beta, gamma);
            s.snake.setPoints(pts);
            s.snakeInited = true;

            RGBImage ov = overlay_connected_contour(s.current, pts, 0, 255, 0, 1, true);
            
            std::string b64 = rgb_to_base64_png(ov);
            
            json j;
            j["success"] = true;
            j["message"] = "Snake initialized";
            j["num_points"] = (int)pts.size();
            j["overlay"] = b64;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            std::cout << "[Snake Init] ERROR: " << e.what() << std::endl;
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/snake/evolve", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int iterations = form_int(req, "iterations", 100);
        bool restart = form_bool(req, "restart", false);
        float alpha = form_float(req, "alpha", NAN);
        float beta  = form_float(req, "beta", NAN);
        float gamma = form_float(req, "gamma", NAN);
        
        std::cout << "[Snake Evolve] Session: " << sid
                  << ", Iterations: " << iterations
                  << ", Restart: " << (restart ? "true" : "false")
                  << std::endl;
        
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        if (!s.snakeInited) {
            res.status = 400;
            res.set_content(json{{"error","snake not initialized"}}.dump(), "application/json");
            return;
        }

        if (!std::isnan(alpha)) s.snake.alpha = alpha;
        if (!std::isnan(beta))  s.snake.beta  = beta;
        if (!std::isnan(gamma)) s.snake.gamma = gamma;
        if (restart) s.snake.reset();

        auto history = s.snake.evolve(iterations);
        auto& contour = s.snake.points;

        RGBImage ov = overlay_connected_contour(s.current, contour, 0, 255, 255, 1, true);
        
        json jc = json::array();
        for (auto& p : contour) jc.push_back({(int)roundf(p.first), (int)roundf(p.second)});

        std::string b64 = rgb_to_base64_png(ov);
        
        json j;
        j["success"] = true;
        j["contour"] = jc;
        j["overlay"] = b64;
        j["iterations"] = (int)history.size();
        j["params"] = {
            {"alpha", s.snake.alpha},
            {"beta", s.snake.beta},
            {"gamma", s.snake.gamma},
            {"restart", restart}
        };
        res.set_content(j.dump(), "application/json");
    });

    // ========== Contour Analysis ==========

    svr.Post("/analyze-contour", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        
        ContourAnalysis ca;
        
        if (s.snakeInited && s.snake.points.size() >= 3) {
            // Use snake contour
            auto snakePts = s.snake.points;
            std::vector<std::pair<int,int>> intPts;
            for (auto& p : snakePts) {
                intPts.push_back({(int)roundf(p.first), (int)roundf(p.second)});
            }
            ca.boundary = intPts;
            ca.numPoints = (int)intPts.size();
            ca.isClosed = true;
            
            // Calculate perimeter
            float perim = 0;
            for (size_t i = 0; i < snakePts.size(); i++) {
                size_t j = (i + 1) % snakePts.size();
                float dx = snakePts[j].first - snakePts[i].first;
                float dy = snakePts[j].second - snakePts[i].second;
                perim += sqrtf(dx*dx + dy*dy);
            }
            ca.perimeter = perim;
            
            // Area via shoelace
            float area = 0;
            for (size_t i = 0; i < intPts.size(); i++) {
                size_t j = (i + 1) % intPts.size();
                area += intPts[i].first * intPts[j].second - intPts[j].first * intPts[i].second;
            }
            ca.area = fabsf(area) / 2.0f;
            
            // Compute Freeman chain code from boundary points.
            // For correct visualization, build a dense boundary in lockstep
            // with chain-code steps so indices map 1:1.
            const int ddx[] = {1, 1, 0, -1, -1, -1, 0, 1};
            const int ddy[] = {0, 1, 1, 1, 0, -1, -1, -1};
            std::vector<std::pair<int,int>> denseBoundary;

            for (size_t i = 0; i < intPts.size(); i++) {
                size_t j = (i + 1) % intPts.size();
                int dx = intPts[j].first - intPts[i].first;
                int dy = intPts[j].second - intPts[i].second;
                int steps = std::max(std::abs(dx), std::abs(dy));
                if (steps > 0) {
                    for (int s = 0; s < steps; s++) {
                        int cx = intPts[i].first + (dx * s) / steps;
                        int cy = intPts[i].second + (dy * s) / steps;
                        int nx = intPts[i].first + (dx * (s + 1)) / steps;
                        int ny = intPts[i].second + (dy * (s + 1)) / steps;
                        
                        int dirDx = nx - cx;
                        int dirDy = ny - cy;
                        
                        // Find direction code
                        for (int d = 0; d < 8; d++) {
                            if (ddx[d] == dirDx && ddy[d] == dirDy) {
                                if (denseBoundary.empty() ||
                                    denseBoundary.back().first != cx ||
                                    denseBoundary.back().second != cy) {
                                    denseBoundary.push_back({cx, cy});
                                }
                                ca.chainCode.push_back(d);
                                break;
                            }
                        }
                    }
                }
            }

            if (!denseBoundary.empty()) {
                ca.boundary = std::move(denseBoundary);
                ca.numPoints = (int)ca.boundary.size();
            }

            std::cout << "[/analyze-contour] Computed chain code with " << ca.chainCode.size() << " codes" << std::endl;
        } else {
            // Use Canny edges
            GrayImage gray = to_gray(s.current);
            GrayImage edges = canny(gray, 1.0f, 0.05f, 0.15f);
            ca = analyze_contour(edges);
        }

        // Render visualizations
        RGBImage freemanOverlay = render_freeman_overlay(s.current, ca);
        RGBImage freemanCodeImg = render_freeman_code_image(ca, s.current.w, s.current.h);

        // Build chain code string
        std::string chainCodeStr;
        for (int code : ca.chainCode) {
            chainCodeStr += std::to_string(code);
        }

        json jb = json::array();
        for (auto& p : ca.boundary) jb.push_back({p.first, p.second});

        json j;
        j["success"] = true;
        j["analysis"] = {
            {"boundary", jb},
            {"chain_code", chainCodeStr},
            {"chain_code_length", (int)ca.chainCode.size()},
            {"perimeter", ca.perimeter},
            {"area", ca.area},
            {"num_points", ca.numPoints},
            {"is_closed", ca.isClosed}
        };
        j["freeman_overlay"] = rgb_to_base64_png(freemanOverlay);
        j["freeman_code_image"] = rgb_to_base64_png(freemanCodeImg);
        res.set_content(j.dump(), "application/json");
    });

    // ========== Session Management ==========

    svr.Post("/reset", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        s.current = s.original;
        s.snakeInited = false;
        s.history.clear();
        s.contourPts.clear();
        std::string b64 = rgb_to_base64_png(s.original);
        json j;
        j["success"] = true;
        j["original"] = b64;
        j["processed"] = b64;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/undo", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        if (!s.history.empty()) s.history.pop_back();
        s.current = s.original;
        std::string b64 = rgb_to_base64_png(s.original);
        json j;
        j["success"] = true;
        j["original"] = b64;
        j["processed"] = b64;
        j["history_length"] = (int)s.history.size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/save", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        std::string b64 = rgb_to_base64_png(s.current);
        json j;
        j["success"] = true;
        j["image"] = b64;
        j["format"] = "PNG";
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/clear", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        std::lock_guard<std::mutex> lk(g_mutex);
        g_sessions.erase(sid);
        json j;
        j["success"] = true;
        j["message"] = "Session cleared";
        res.set_content(j.dump(), "application/json");
    });

    std::cout << "========================================" << std::endl;
    std::cout << "FromScratchCV C++ Server v2.0" << std::endl;
    std::cout << "Enhanced shape detection enabled" << std::endl;
    std::cout << "Listening on http://0.0.0.0:8000" << std::endl;
    std::cout << "========================================" << std::endl;
    
    svr.listen("0.0.0.0", 8000);
    return 0;
}