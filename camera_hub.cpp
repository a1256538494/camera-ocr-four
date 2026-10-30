#include "camera_hub.h"
#include "image_convert.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

CameraHub::CameraHub(QObject *parent)
    : QObject(parent)
{
    for (int i = 0; i < kCamCount; ++i) {
        sessions[i].id = i + 1;
        sessions[i].stateText = QStringLiteral("空闲");
    }
}

void CameraHub::setCamTitle(int camId, const QString &title)
{
    /*
     * 等价于：
     *   CameraSession *s = session(camId);   // ① 初始化
     *   if (s)                               // ② 条件：指针非空即真
     *   s->title = title;
     */
    if (CameraSession *s = session(camId))
        s->title = title;
}

QString CameraHub::resolveVideoPath(int camIndex) const
{
    // Cam01→test1.mp4 … Cam04→test4.mp4
    const int n = qBound(camIndex, 1, kCamCount);
    const QString fileName = QStringLiteral("test%1.mp4").arg(n);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QStringLiteral("D:/tool/Qt/Project/modbusCamera-main/test_picture_video/") + fileName,
        appDir + QStringLiteral("/test_picture_video/") + fileName,
        appDir + QStringLiteral("/../../../../test_picture_video/") + fileName,
        QDir::currentPath() + QStringLiteral("/test_picture_video/") + fileName,
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path))
            return QFileInfo(path).absoluteFilePath();
    }
    return QFileInfo(candidates.first()).absoluteFilePath();
}

bool CameraHub::openAll()
{
    int ok = 0;
    for (int i = 0; i < kCamCount; ++i) {
        CameraSession &s = sessions[i];
        if (s.cap.isOpened())
            s.cap.release();
        s.videoPath = resolveVideoPath(s.id);
        s.opened = s.cap.open(s.videoPath.toStdString());
        s.triggerPending = false;
        s.countOk = 0;
        s.frameCount = 0;
        if (!s.opened) {
            qDebug() << "视频打开失败 Cam" << s.id << s.videoPath;
            emit camOpenFailed(s.id, s.videoPath);
        } else {
            ++ok;
            emit camOpened(s.id, s.videoPath);
        }
    }
    captureTickCount = 0;
    return ok > 0;
}

void CameraHub::closeAll()
{
    for (auto &s : sessions) {
        if (s.cap.isOpened())
            s.cap.release();
        s.opened = false;
        s.lastMat.release();
        s.lastPreview = QImage();
        s.lastAnnotated = QImage();
        s.holdAnnotated = false;
        s.triggerPending = false;
        s.countOk = 0;
        s.frameCount = 0;
    }
}

void CameraHub::captureTick()
{
    ++captureTickCount;

    for (auto &s : sessions) {
        if (!s.opened || !s.cap.isOpened())
            continue;

        cv::Mat frame;
        if (!s.cap.read(frame) || frame.empty()) {
            s.cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            continue;
        }

        s.lastMat = frame.clone();
        s.lastPreview = matToQImage(frame);
        ++s.frameCount;

        if (s.holdAnnotated && s.annotatedHoldTimer.isValid()
            && s.annotatedHoldTimer.elapsed() >= kAnnotatedHoldMs) {
            s.holdAnnotated = false;
        }

        emit frameCaptured(s.id);
    }

    if (captureTickCount % 30 == 0) {
        for (const auto &s : sessions) {
            if (s.opened)
                emit fpsHint(s.id, 1000.0 / kCaptureIntervalMs);
        }
    }
}

bool CameraHub::isOpened(int camId) const
{
    const CameraSession *s = session(camId);
    return s && s->opened;
}

int CameraHub::openedCount() const
{
    int n = 0;
    for (const auto &s : sessions) {
        if (s.opened)
            ++n;
    }
    return n;
}
//返回指向CameraSession的指针
CameraSession *CameraHub::session(int camId)
{
    if (camId < 1 || camId > kCamCount)
        return nullptr;
    return &sessions[camId - 1];
}

const CameraSession *CameraHub::session(int camId) const
{
    if (camId < 1 || camId > kCamCount)
        return nullptr;
    return &sessions[camId - 1];
}

QImage CameraHub::displayImage(int camId) const
{
    const CameraSession *s = session(camId);
    if (!s)
        return {};
    return s->displayImage(kAnnotatedHoldMs);
}
