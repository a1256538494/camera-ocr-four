#include "ocr_worker.h"
#include "ocrDetect.h"
#include "image_convert.h"

#include <QMetaObject>

OcrWorker::OcrWorker(QObject *parent)
    : QObject(parent)
{
}

OcrWorker::~OcrWorker() = default;

void OcrWorker::ensureEngine()
{
    if (!ocrEng)
        ocrEng = std::make_unique<ocrDetect>();
}

bool OcrWorker::epochValid(int epoch) const
{
    return !abortFlag.load() && epoch == workEpoch.load();
}

void OcrWorker::clearPending()
{
    QMutexLocker locker(&mutex);
    pendingFrames.clear();
}

void OcrWorker::requestAbort()
{
    workEpoch.fetch_add(1);
    abortFlag.store(true);
    QMutexLocker locker(&mutex);
    pendingFrames.clear();
}

void OcrWorker::resetAbort()
{
    abortFlag.store(false);
    QMutexLocker locker(&mutex);
    pendingFrames.clear();
}

void OcrWorker::processFrame(int camId, cv::Mat frame, int epoch)
{
    if (!epochValid(epoch))
        return;

    {
        QMutexLocker locker(&mutex);
        if (busy) {
            pendingFrames.insert(camId, frame);
            return;
        }
        busy = true;
    }
    emit busyChanged(true);
    runDetect(camId, std::move(frame), epoch);
}

void OcrWorker::runDetect(int camId, cv::Mat frame, int epoch)
{
    if (!epochValid(epoch)) {
        {
            QMutexLocker locker(&mutex);
            busy = false;
            pendingFrames.clear();
        }
        emit busyChanged(false);
        return;
    }

    int code = 0;
    ensureEngine();
    if (!frame.empty() && ocrEng)
        code = ocrEng->getDetectResult(frame); // 可能较久；退出时等它自然结束，勿 terminate

    if (!epochValid(epoch)) {
        {
            QMutexLocker locker(&mutex);
            busy = false;
            pendingFrames.clear();
        }
        emit busyChanged(false);
        return;
    }

    emit resultReady(camId, code, matToQImage(frame));
    drainPending(epoch);
}

void OcrWorker::drainPending(int epoch)
{
    if (!epochValid(epoch)) {
        {
            QMutexLocker locker(&mutex);
            busy = false;
            pendingFrames.clear();
        }
        emit busyChanged(false);
        return;
    }

    int nextId = 0;
    cv::Mat nextFrame;
    {
        QMutexLocker locker(&mutex);
        if (pendingFrames.isEmpty()) {
            busy = false;
        } else {
            auto it = pendingFrames.begin();
            nextId = it.key();
            nextFrame = it.value();
            pendingFrames.erase(it);
        }
    }

    if (nextFrame.empty()) {
        emit busyChanged(false);
        return;
    }

    QMetaObject::invokeMethod(this, [this, nextId, nextFrame, epoch]() mutable {
        if (!epochValid(epoch)) {
            {
                QMutexLocker locker(&mutex);
                busy = false;
                pendingFrames.clear();
            }
            emit busyChanged(false);
            return;
        }
        runDetect(nextId, nextFrame, epoch);
    }, Qt::QueuedConnection);
}
