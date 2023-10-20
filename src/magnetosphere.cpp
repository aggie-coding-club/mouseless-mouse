#include "magnetosphere.h"

Magnetosphere::Magnetosphere(Eigen::Vector3f startingPoint) {
    points << startingPoint;
}

void Magnetosphere::addPoint(Eigen::Vector3f point) {
    points.conservativeResize(3, points.cols() + 1);
    points.col(points.cols() - 1) = point;
}

void Magnetosphere::calibrate() {
    // I saw the prison of the angels 
    // and their stations beyond the firmaments were onto fiery lights
    // my face was changed and I could no longer behold
    // I saw the hidden and the visible path
    float xMedianFind[points.cols()];
    float yMedianFind[points.cols()];
    float zMedianFind[points.cols()];

    for (int i = 0; i < points.cols(); i++) {
        xMedianFind[i] = points.coeff(i, 0);
        yMedianFind[i] = points.coeff(i, 1);
        zMedianFind[i] = points.coeff(i, 2);
    }

    for (int i = 0; i < points.cols() - 1; i++) {
        if (xMedianFind[i] > xMedianFind[i + 1]) {
            float tempFloat = xMedianFind[i];
            xMedianFind[i] = xMedianFind[i + 1];
            xMedianFind[i + 1] = tempFloat;
            i -= 2;
            if (i == -2) i++;
        }
    }

    for (int i = 0; i < points.cols() - 1; i++) {
        if (yMedianFind[i] > yMedianFind[i + 1]) {
            float tempFloat = yMedianFind[i];
            yMedianFind[i] = yMedianFind[i + 1];
            yMedianFind[i + 1] = tempFloat;
            i -= 2;
            if (i == -2) i++;
        }
    }

    for (int i = 0; i < points.cols() - 1; i++) {
        if (zMedianFind[i] > zMedianFind[i + 1]) {
            float tempFloat = zMedianFind[i];
            zMedianFind[i] = zMedianFind[i + 1];
            zMedianFind[i + 1] = tempFloat;
            i -= 2;
            if (i == -2) i++;
        }
    }

    centroid = {xMedianFind[points.cols() / 2], yMedianFind[points.cols() / 2], zMedianFind[points.cols() / 2]};

    Eigen::Matrix3Xf lines(3, points.cols());
    Eigen::Matrix3Xf rotation(3, 3);

    lines = points.colwise() - centroid;
    rotation.col(0) = lines.col(0);
    float least_squares_transform = 0;

    for (int i = 0; i < points.cols(); i++) {
        Eigen::Vector3f x = points.col(i) - centroid;
        Eigen::Vector3f u = rotation.col(0);
        Eigen::Vector3f projection = (x.dot(u) / u.dot(u)) * u;
        projection = x - projection;

        least_squares_transform += projection.squaredNorm();
    }

    for (int i = 1; i < lines.cols(); i++) {
        Eigen::Vector3f u = lines.col(i);
        float least_squares_compare = 0;

        for (int j = 0; j < points.cols(); i++) {
            Eigen::Vector3f x = points.col(j) - centroid;
            Eigen::Vector3f projection = (x.dot(u) / u.dot(u)) * u;
            projection = x - projection;

            least_squares_compare += projection.squaredNorm();
        }

        if (least_squares_compare < least_squares_transform) {
            rotation.col(0) = u;
            least_squares_transform = least_squares_compare;
        }
    }

    Eigen::Vector3f OZZ = {1, 0 ,0};
    rotation.col(1) = rotation.col(0).cross(OZZ);
    rotation.col(2) = rotation.col(0).cross(rotation.col(1));

    transform = rotation.inverse();

    float least_squares_squish = 0;
    Eigen::Matrix <float, 3, 3> elipsoidSquishing;

    elipsoidSquishing.col(0) = points.col(0);
    elipsoidSquishing.col(1) = points.col(0);
    elipsoidSquishing.col(2) = points.col(0);
    elipsoidSquishing(0, 1) = 0;
    elipsoidSquishing(0, 2) = 0;
    elipsoidSquishing(1, 0) = 0;
    elipsoidSquishing(1, 2) = 0;
    elipsoidSquishing(2, 0) = 0;
    elipsoidSquishing(2, 1) = 0;

    for (int i = 0; i < points.cols(); i++) {
        float mult = elipsoidSquishing(0, 0) * elipsoidSquishing(1, 1) * elipsoidSquishing(2, 2);
        Eigen::Vector3f vect = points.col(i);
        vect.row(0) *= (float) elipsoidSquishing(0, 0);
        vect.row(1) *= (float) elipsoidSquishing(1, 1);
        vect.row(2) *= (float) elipsoidSquishing(2, 2);
        mult /= sqrt(vect.squaredNorm());

        least_squares_squish = pow(mult - 1, 2);
    }

    for (int i = 1; i < points.cols(); i++) {
        float mult = points(i, 0) * elipsoidSquishing(1, 1) * elipsoidSquishing(2, 2);
        float temp_least_squares = 0;

        for (int j = 0; j < points.cols(); j++) {
            Eigen::Vector3f vect = points.col(i);
            vect.row(0) *= (float) points(i, 0);
            vect.row(1) *= (float) elipsoidSquishing(1, 1);
            vect.row(2) *= (float) elipsoidSquishing(2, 2);
            mult /= sqrt(vect.squaredNorm());

            temp_least_squares = pow(mult - 1, 2);
        }

        if (temp_least_squares < least_squares_squish) {
            temp_least_squares = least_squares_squish;
            elipsoidSquishing(0, 0) = points(i, 0);
        }

        mult = points(i, 1) * elipsoidSquishing(0, 0) * elipsoidSquishing(2, 2);
        temp_least_squares = 0;

        for (int j = 0; j < points.cols(); j++) {
            Eigen::Vector3f vect = points.col(i);
            vect.row(1) *= (float) points(i, 1);
            vect.row(0) *= (float) elipsoidSquishing(0, 0);
            vect.row(2) *= (float) elipsoidSquishing(2, 2);
            mult /= sqrt(vect.squaredNorm());

            temp_least_squares = pow(mult - 1, 2);
        }

        if (temp_least_squares < least_squares_squish) {
            temp_least_squares = least_squares_squish;
            elipsoidSquishing(1, 1) = points(i, 1);
        }

        mult = points(i, 2) * elipsoidSquishing(0, 0) * elipsoidSquishing(1, 1);
        temp_least_squares = 0;

        for (int j = 0; j < points.cols(); j++) {
            Eigen::Vector3f vect = points.col(i);
            vect.row(2) *= (float) points(i, 2);
            vect.row(1) *= (float) elipsoidSquishing(1, 1);
            vect.row(0) *= (float) elipsoidSquishing(0, 0);
            mult /= sqrt(vect.squaredNorm());

            temp_least_squares = pow(mult - 1, 2);
        }

        if (temp_least_squares < least_squares_squish) {
            temp_least_squares = least_squares_squish;
            elipsoidSquishing(2, 2) = points(i, 2);
        }
    }
    
    transform *= elipsoidSquishing;
    transform *= rotation;
}