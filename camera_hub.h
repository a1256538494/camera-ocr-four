#ifndef CAMERA_HUB_H
#define CAMERA_HUB_H

#include <QObject>
#include <QString>
#include <array>

#include "camera_session.h"

// 本地视频四路采集（当前阶段不做真实相机 / Modbus）
class CameraHub : public QObject
{
    Q_OBJECT
public:
    static constexpr int kCamCount = 4;
    static constexpr int kCaptureIntervalMs = 33;
    static constexpr int kAnnotatedHoldMs = 1500;

    explicit CameraHub(QObject *parent = nullptr);

    void setCamTitle(int camId, const QString &title);
    bool openAll();
    void closeAll();
    void captureTick(); // 由外部 Timer 调用

    bool isOpened(int camId) const;
    int openedCount() const;
    CameraSession *session(int camId);
    const CameraSession *session(int camId) const;
    QImage displayImage(int camId) const;

signals:
    void camOpened(int camId, const QString &path);
    void camOpenFailed(int camId, const QString &path);
    void frameCaptured(int camId); // 有新帧（详情窗可跟刷）
    void fpsHint(int camId, double fps);

private:
    QString resolveVideoPath(int camIndex) const;

    std::array<CameraSession, kCamCount> sessions;
    qint64 captureTickCount = 0;
};

#endif // CAMERA_HUB_H
