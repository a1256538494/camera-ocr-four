#ifndef PADDLE_OCR_TOOLS
#include "clipper.hpp"
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

inline bool cvPointCompare(const cv::Point& a, const cv::Point& b) {
    return a.x < b.x;
}
//inVec：原始点集
inline std::vector<cv::Point> getMinBoxes(const std::vector<cv::Point>& inVec,
    float& minSideLen,
    float& allEdgeSize) {
    std::vector<cv::Point> minBoxVec;
    //minAreaRect：找到能包围住 inVec 里所有点的面积最小的矩形，寻找最小外接旋转矩形
    cv::RotatedRect textRect = cv::minAreaRect(inVec);
    cv::Mat boxPoints2f;
    //boxPoints：opencv转换函数，接收只有中心点、宽高、角度的 textRect，经过三角函数计算，推导出这个矩形到底在图像上的哪 4 个位置
    cv::boxPoints(textRect, boxPoints2f);

    float* p1 = (float*)boxPoints2f.data;
    std::vector<cv::Point> tmpVec;
    for (int i = 0; i < 4; ++i, p1 += 2) {
        tmpVec.emplace_back(int(p1[0]), int(p1[1]));
    }

    std::sort(tmpVec.begin(), tmpVec.end(), cvPointCompare);

    minBoxVec.clear();

    int index1, index2, index3, index4;
    if (tmpVec[1].y > tmpVec[0].y) {
        index1 = 0;
        index4 = 1;
    }
    else {
        index1 = 1;
        index4 = 0;
    }

    if (tmpVec[3].y > tmpVec[2].y) {
        index2 = 2;
        index3 = 3;
    }
    else {
        index2 = 3;
        index3 = 2;
    }

    minBoxVec.clear();

    minBoxVec.push_back(tmpVec[index1]);
    minBoxVec.push_back(tmpVec[index2]);
    minBoxVec.push_back(tmpVec[index3]);
    minBoxVec.push_back(tmpVec[index4]);

    minSideLen = (std::min)(textRect.size.width, textRect.size.height);
    // allEdgeSize = 2.f * (textRect.size.width + textRect.size.height);
    allEdgeSize = static_cast<float>(cv::arcLength(inVec, true));
    return minBoxVec;
}

inline std::vector<cv::Point> unClip(const std::vector<cv::Point>& inBox,
    float perimeter, float unClipRatio) {
    std::vector<cv::Point> outBox;
    ClipperLib::Path poly;

    for (int i = 0; i < inBox.size(); ++i) {
        poly.push_back(ClipperLib::IntPoint(inBox[i].x, inBox[i].y));
    }

    double distance = unClipRatio * ClipperLib::Area(poly) / (double)perimeter;

    ClipperLib::ClipperOffset clipperOffset;
    clipperOffset.AddPath(poly, ClipperLib::JoinType::jtRound,
        ClipperLib::EndType::etClosedPolygon);
    ClipperLib::Paths polys;
    polys.push_back(poly);
    clipperOffset.Execute(polys, distance);

    outBox.clear();
    std::vector<cv::Point> rsVec;
    for (int i = 0; i < polys.size(); ++i) {
        ClipperLib::Path tmpPoly = polys[i];
        for (int j = 0; j < tmpPoly.size(); ++j) {
            outBox.emplace_back(static_cast<int>(tmpPoly[j].X), static_cast<int>(tmpPoly[j].Y));
        }
    }
    return outBox;
}

inline float boxScoreFast(const cv::Mat& inMat,
    const std::vector<cv::Point>& inBox) {
    std::vector<cv::Point> box = inBox;
    int width = inMat.cols; // 原图宽度
    int height = inMat.rows;// 原图高度
    int maxX = -1, minX = 1000000, maxY = -1, minY = 1000000;
    for (int i = 0; i < box.size(); ++i) {
        if (maxX < box[i].x)
            maxX = box[i].x;
        if (minX > box[i].x)
            minX = box[i].x;
        if (maxY < box[i].y)
            maxY = box[i].y;
        if (minY > box[i].y)
            minY = box[i].y;
    }
    maxX = (std::min)((std::max)(maxX, 0), width - 1);
    minX = (std::max)((std::min)(minX, width - 1), 0);
    maxY = (std::min)((std::max)(maxY, 0), height - 1);
    minY = (std::max)((std::min)(minY, height - 1), 0);

    // 坐标系平移
    for (int i = 0; i < box.size(); ++i) {
        box[i].x = box[i].x - minX;
        box[i].y = box[i].y - minY;
    }

    std::vector<std::vector<cv::Point>> maskBox;
    maskBox.push_back(box);
    cv::Mat maskMat(maxY - minY + 1, maxX - minX + 1, CV_8UC1,
        cv::Scalar(0, 0, 0));
    cv::fillPoly(maskMat, maskBox, cv::Scalar(1, 1, 1), 1);
    return static_cast<float>(cv::mean(inMat(cv::Rect(cv::Point(minX, minY),
        cv::Point(maxX + 1, maxY + 1)))
        .clone(),
        maskMat)
        .val[0]);
}

inline cv::Mat getRotateCropImage(const cv::Mat& src,
    std::vector<cv::Point> box) {
    cv::Mat image;
    src.copyTo(image);
    std::vector<cv::Point> points = box;

    int collectX[4] = { box[0].x, box[1].x, box[2].x, box[3].x };
    int collectY[4] = { box[0].y, box[1].y, box[2].y, box[3].y };
    int left = int(*std::min_element(collectX, collectX + 4));
    int right = int(*std::max_element(collectX, collectX + 4));
    int top = int(*std::min_element(collectY, collectY + 4));
    int bottom = int(*std::max_element(collectY, collectY + 4));

    cv::Mat imgCrop;
    //画出ROI区域，参数分别是 (左上角X, 左上角Y, 宽度, 高度)
    //重载了函数调用运算符 operator()
    image(cv::Rect(left, top, right - left, bottom - top)).copyTo(imgCrop);
    for (int i = 0; i < points.size(); i++) {
        points[i].x -= left;
        points[i].y -= top;
    }

    int imgCropWidth = int(sqrt(pow(points[0].x - points[1].x, 2) +
        pow(points[0].y - points[1].y, 2)));
    int imgCropHeight = int(sqrt(pow(points[0].x - points[3].x, 2) +
        pow(points[0].y - points[3].y, 2)));

    cv::Point2f ptsDst[4];
    ptsDst[0] = cv::Point2f(0.f, 0.f);
    ptsDst[1] = cv::Point2f(static_cast<float>(imgCropWidth), 0.f);
    ptsDst[2] = cv::Point2f(static_cast<float>(imgCropWidth), static_cast<float>(imgCropHeight));
    ptsDst[3] = cv::Point2f(0.f, static_cast<float>(imgCropHeight));

    cv::Point2f ptsSrc[4];
    ptsSrc[0] = cv::Point2f(static_cast<float>(points[0].x), static_cast<float>(points[0].y));
    ptsSrc[1] = cv::Point2f(static_cast<float>(points[1].x), static_cast<float>(points[1].y));
    ptsSrc[2] = cv::Point2f(static_cast<float>(points[2].x), static_cast<float>(points[2].y));
    ptsSrc[3] = cv::Point2f(static_cast<float>(points[3].x), static_cast<float>(points[3].y));

    //计算数学变换矩阵
    cv::Mat M = cv::getPerspectiveTransform(ptsSrc, ptsDst);

    cv::Mat partImg;
    //拿着变换矩阵 M，把 imgCrop 里面原本处于 ptsSrc（倾斜四个角）位置的像素重新排布到 ptsDst（方正四个角）的位置上
    cv::warpPerspective(imgCrop, partImg, M,
        cv::Size(imgCropWidth, imgCropHeight),
        cv::BORDER_REPLICATE);

    if (float(partImg.rows) >= float(partImg.cols) * 1.5) {
        cv::Mat srcCopy = cv::Mat(partImg.rows, partImg.cols, partImg.depth());
        cv::transpose(partImg, srcCopy);
        cv::flip(srcCopy, srcCopy, 0);
        return srcCopy;
    }
    else {
        return partImg;
    }
}
#endif // DEBUG