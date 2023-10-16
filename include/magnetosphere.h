#ifndef MAGNETOSPHERE_H
#define MAGNETOSPHERE_H

#include <ArduinoEigen/Eigen/Dense>

class Magnetosphere {
public:
    Magnetosphere(Eigen::Vector3f);
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