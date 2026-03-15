/*
 * main.cpp — FromScratchCV HTTP Server
 * Only contains the server setup and endpoint handlers.
 * All CV algorithms are in separate files.
 */

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
        j["version"] = "1.0.0";
        j["endpoints"] = {"/upload","/canny","/hough-lines","/hough-circles",
                          "/detect-ellipses","/snake/init","/snake/evolve",
                          "/snake/reset","/analyze-contour"};
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
        float lo = std::stof(form_val(req, "low_threshold", "0.05"));
        float hi = std::stof(form_val(req, "high_threshold", "0.15"));
        float sigma = std::stof(form_val(req, "sigma", "1.0"));

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

    // ========== Shape Detection ==========

    svr.Post("/hough-lines", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int threshold = std::stoi(form_val(req, "threshold", "50"));
        float thetaRes = std::stof(form_val(req, "theta_res", "1.0"));
        float rhoRes = std::stof(form_val(req, "rho_res", "1.0"));

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, 1.0f, 0.05f, 0.15f);
        auto lines = hough_lines(edges, thetaRes, rhoRes, threshold);

        RGBImage ov = overlay_lines(s.current, lines, 0, 0, 255);
        json jlines = json::array();
        for (auto& l : lines) jlines.push_back({l.rho, l.theta});

        json j;
        j["success"] = true;
        j["lines"] = jlines;
        j["overlay"] = rgb_to_base64_png(ov);
        j["num_lines"] = (int)lines.size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/hough-circles", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int rMin = std::stoi(form_val(req, "radius_min", "10"));
        int rMax = std::stoi(form_val(req, "radius_max", "100"));
        float thr = std::stof(form_val(req, "threshold", "0.5"));

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, 1.0f, 0.05f, 0.15f);
        auto circles = hough_circles(edges, rMin, rMax, thr);

        RGBImage ov = overlay_circles(s.current, circles, 0, 255, 0);
        json jc = json::array();
        for (auto& c : circles) jc.push_back({c.x, c.y, c.r});

        json j;
        j["success"] = true;
        j["circles"] = jc;
        j["overlay"] = rgb_to_base64_png(ov);
        j["num_circles"] = (int)circles.size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/detect-ellipses", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        int minA = std::stoi(form_val(req, "min_area", "100"));
        int maxA = std::stoi(form_val(req, "max_area", "10000"));

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, 1.0f, 0.05f, 0.15f);
        auto els = detect_ellipses(edges, minA, maxA);

        RGBImage ov = overlay_ellipses(s.current, els, 255, 0, 0);
        json je = json::array();
        for (auto& e : els)
            je.push_back({{"x",e.x},{"y",e.y},{"a",e.a},{"b",e.b},{"angle",e.angle}});

        json j;
        j["success"] = true;
        j["ellipses"] = je;
        j["overlay"] = rgb_to_base64_png(ov);
        j["num_ellipses"] = (int)els.size();
        res.set_content(j.dump(), "application/json");
    });

    // ========== Active Contour ==========

    svr.Post("/snake/init", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
        auto ptsStr = req.form.get_field("points");
        float alpha = std::stof(form_val(req, "alpha", "0.5"));
        float beta  = std::stof(form_val(req, "beta",  "0.5"));
        float gamma = std::stof(form_val(req, "gamma", "1.0"));

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_sessions.find(sid) == g_sessions.end()) {
            res.status = 404;
            res.set_content(json{{"error","session not found"}}.dump(), "application/json");
            return;
        }
        Session& s = g_sessions[sid];
        json jp = json::parse(ptsStr);
        std::vector<std::pair<float,float>> pts;
        for (auto& p : jp) pts.push_back({p[0].get<float>(), p[1].get<float>()});

        GrayImage gray = to_gray(s.current);
        s.snake.init(gray, alpha, beta, gamma);
        s.snake.setPoints(pts);
        s.snakeInited = true;

        RGBImage ov = overlay_points(s.current, pts, 0, 255, 0);
        json j;
        j["success"] = true;
        j["message"] = "Snake initialized";
        j["num_points"] = (int)pts.size();
        j["overlay"] = rgb_to_base64_png(ov);
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/snake/evolve", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
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
        auto history = s.snake.evolve();
        auto& contour = s.snake.points;

        RGBImage ov = overlay_points(s.current, contour, 0, 255, 255, 2);
        json jc = json::array();
        for (auto& p : contour) jc.push_back({(int)roundf(p.first), (int)roundf(p.second)});

        json j;
        j["success"] = true;
        j["contour"] = jc;
        j["overlay"] = rgb_to_base64_png(ov);
        j["iterations"] = (int)history.size();
        j["history_length"] = (int)history.size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/snake/reset", [](const httplib::Request& req, httplib::Response& res) {
        auto sid = req.form.get_field("session_id");
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
        s.snake.reset();
        json jc = json::array();
        for (auto& p : s.snake.points) jc.push_back({(int)roundf(p.first), (int)roundf(p.second)});
        json j;
        j["success"] = true;
        j["contour"] = jc;
        j["message"] = "Snake reset";
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
        GrayImage gray = to_gray(s.current);
        GrayImage edges = canny(gray, 1.0f, 0.05f, 0.15f);
        auto ca = analyze_contour(edges);

        json jb = json::array();
        for (auto& p : ca.boundary) jb.push_back({p.first, p.second});

        json j;
        j["success"] = true;
        j["analysis"] = {
            {"boundary", jb},
            {"chain_code", ca.chainCode},
            {"perimeter", ca.perimeter},
            {"area", ca.area},
            {"num_points", ca.numPoints},
            {"is_closed", ca.isClosed}
        };
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

    std::cout << "FromScratchCV C++ server listening on http://0.0.0.0:8000\n";
    svr.listen("0.0.0.0", 8000);
    return 0;
}
