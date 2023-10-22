#ifndef MAGNETOSPHERE_H
#define MAGNETOSPHERE_H

#include <ArduinoEigen/Eigen/Dense>
#include <iostream>
#include <fstream>

class Magnetosphere {
public:
    Magnetosphere();
    Magnetosphere(std::string filename);
    void addPoint(Eigen::Vector3f);
    void calibrate();

    Eigen::Vector3f adjustPoint(Eigen::Vector3f);
    void getCentroid();
    void getTransformMatrix();
private:
    Eigen::Matrix3Xf points;
    Eigen::Vector3f centroid;
    Eigen::Matrix3Xf transform;
};

#endif