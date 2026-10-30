#ifndef IMAGE_CONVERT_H
#define IMAGE_CONVERT_H

#include <QImage>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

inline QImage matToQImage(const cv::Mat &inMat)
{
    if (inMat.empty())
        return {};

    switch (inMat.type()) {
    case CV_8UC4: {
        QImage image(inMat.data, inMat.cols, inMat.rows,
                     static_cast<int>(inMat.step), QImage::Format_ARGB32);
        return image.copy();
    }
    case CV_8UC3: {
        // OpenCV BGR → QImage RGB
        cv::Mat rgb;
        cv::cvtColor(inMat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step), QImage::Format_RGB888);
        return image.copy();
    }
    case CV_8UC1: {
        QImage image(inMat.data, inMat.cols, inMat.rows,
                     static_cast<int>(inMat.step), QImage::Format_Grayscale8);
        return image.copy();
    }
    default:
        break;
    }
    return {};
}

#endif // IMAGE_CONVERT_H
