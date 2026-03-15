#pragma once
/*
 * active_contour.h — Greedy Snake (Active Contour)
 */

#include "image_utils.h"
#include <vector>
#include <utility>

struct Snake {
    std::vector<std::pair<float,float>> points;
    std::vector<std::pair<float,float>> original;
    GrayImage extEnergy;
    float alpha, beta, gamma;
    int winSize, maxIter;

    void init(const GrayImage& gray, float a, float b, float g);
    void setPoints(const std::vector<std::pair<float,float>>& pts);
    std::vector<std::vector<std::pair<float,float>>> evolve();
    void reset();
};
