#ifndef CAMERA_SESSION_H
#define CAMERA_SESSION_H

#include <QImage>
#include <QString>
#include <QElapsedTimer>
#include <opencv2/videoio.hpp>

// 一路会话数据：由 CameraHub 持有；OcrScheduler / 主界面只读写状态与帧缓存
struct CameraSession {
    int id = 0;                      // 1..4
    QString title;
    QString videoPath;
    cv::VideoCapture cap;
    bool opened = false;

    cv::Mat lastMat;            //保存最近抓取的一帧原始图像
    QImage lastPreview;         //保存最近一帧的原始预览图
    QImage lastAnnotated;       //保存最近一帧的标注图
    QElapsedTimer annotatedHoldTimer;
    bool holdAnnotated = false;
    bool triggerPending = false;     // OCR 调度触发位（本地阶段由 100ms 脉冲置位）
    int countOk = 0;                 // 预留：以后写 Modbus 用

    QString stateText = QStringLiteral("空闲");
    double fps = 0.0;
    qint64 frameCount = 0;

    QImage displayImage(int holdMs) const
    {
        if (holdAnnotated && annotatedHoldTimer.isValid()
            && annotatedHoldTimer.elapsed() < holdMs
            && !lastAnnotated.isNull()) {
            return lastAnnotated;
        }
        return lastPreview;
    }
};

#endif // CAMERA_SESSION_H
