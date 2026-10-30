#include "ocr_scheduler.h"
#include "camera_hub.h"
#include "ocr_worker.h"

#include <QMetaObject>
#include <QMetaType>

OcrScheduler::OcrScheduler(CameraHub *hub, QObject *parent)
    : QObject(parent)
    , hub(hub)
{
    //把一个C++类型登记进 Qt 的运行时元类型系统，跨线程，参数必须先"打包"再"投递"
    qRegisterMetaType<cv::Mat>("cv::Mat");
    setupWorker();

    triggerTimer = new QTimer(this);
    connect(triggerTimer, &QTimer::timeout, this, &OcrScheduler::onTriggerPulse);
}

OcrScheduler::~OcrScheduler()
{
    shutdownSync();
}

void OcrScheduler::setupWorker()
{
    ocrThread = new QThread(this);
    ocrWorker = new OcrWorker();
    ocrWorker->moveToThread(ocrThread);

    connect(this, &OcrScheduler::requestOcr, ocrWorker, &OcrWorker::processFrame, Qt::QueuedConnection);
    //void resultReady(int camId, int code, QImage annotated);
    connect(ocrWorker, &OcrWorker::resultReady, this, &OcrScheduler::onOcrResult, Qt::QueuedConnection);
    //void busyChanged(bool busy);
    connect(ocrWorker, &OcrWorker::busyChanged, this, &OcrScheduler::onOcrBusyChanged, Qt::QueuedConnection);
    connect(ocrThread, &QThread::finished, this, &OcrScheduler::onWorkerThreadFinished);

    ocrThread->start();
}

void OcrScheduler::start()
{
    if (running || !hub)
        return;
    shuttingDown = false;
    stopNotified = false;

    if (!ocrThread)
        setupWorker();

    if (!ocrWorker) {
        ocrWorker = new OcrWorker();
        ocrWorker->moveToThread(ocrThread);
        connect(ocrWorker, &OcrWorker::resultReady, this, &OcrScheduler::onOcrResult, Qt::QueuedConnection);
        connect(ocrWorker, &OcrWorker::busyChanged, this, &OcrScheduler::onOcrBusyChanged, Qt::QueuedConnection);
    } else {
        ocrWorker->resetAbort();
    }

    connect(this, &OcrScheduler::requestOcr, ocrWorker, &OcrWorker::processFrame,
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));

    if (!ocrThread->isRunning())
        ocrThread->start();

    running = true;
    emit statusText(QStringLiteral("OCR：100ms 模拟触发（本地视频，未连 Modbus）"));
    triggerTimer->start(kTriggerIntervalMs);
}

void OcrScheduler::stop()
{
    // 软停：与关窗一样掐断投递/丢弃队列，但不退出 OCR 线程（方便再次「全部开始」）
    running = false;
    if (triggerTimer)
        triggerTimer->stop();

    if (hub) {
        for (int id = 1; id <= CameraHub::kCamCount; ++id) {
            if (CameraSession *s = hub->session(id))
                s->triggerPending = false;
        }
    }

    if (ocrWorker) {
        disconnect(this, &OcrScheduler::requestOcr, ocrWorker, &OcrWorker::processFrame);
        ocrWorker->requestAbort(); // bump epoch：已入队的旧 processFrame 全部无效
    }
}

void OcrScheduler::beginStop()
{
    shuttingDown = true;
    running = false;
    if (triggerTimer)
        triggerTimer->stop();

    if (ocrWorker) {
        disconnect(this, &OcrScheduler::requestOcr, ocrWorker, &OcrWorker::processFrame);
        ocrWorker->requestAbort(); // 立刻置位，当前帧结束后不再 drain
    }
}

void OcrScheduler::requestStopAsync()
{
    if (stopNotified)
        return;

    beginStop();

    if (!ocrThread || !ocrThread->isRunning()) {
        if (ocrWorker) {
            delete ocrWorker;
            ocrWorker = nullptr;
        }
        if (!stopNotified) {
            stopNotified = true;
            emit stopped();
        }
        return;
    }

    ocrThread->quit(); // 当前 getDetectResult 结束后事件循环才会退出——不要 terminate
}

void OcrScheduler::onWorkerThreadFinished()
{
    if (ocrWorker) {
        delete ocrWorker;
        ocrWorker = nullptr;
    }
    if (!stopNotified) {
        stopNotified = true;
        emit stopped();
    }
}

void OcrScheduler::shutdownSync()
{
    if (stopNotified && !ocrWorker)
        return;

    beginStop();

    if (ocrThread && ocrThread->isRunning()) {
        ocrThread->quit();
        // 等当前帧 OCR 跑完；禁止 terminate（会打坏 ncnn pool allocator，进程假死）
        if (!ocrThread->wait(20000)) {
            // 极端情况：不再 delete 仍在跑的 worker，交给进程退出回收
            ocrWorker = nullptr;
        }
    }

    if (ocrWorker) {
        delete ocrWorker;
        ocrWorker = nullptr;
    }
    stopNotified = true;
}

void OcrScheduler::onTriggerPulse()
{
    if (!running || shuttingDown || !hub)
        return;

    for (int id = 1; id <= CameraHub::kCamCount; ++id) {
        CameraSession *s = hub->session(id);
        if (s && s->opened && !s->lastMat.empty())
            s->triggerPending = true;
    }
    submitTriggered();
}

void OcrScheduler::submitOcr(int camId)
{
    if (!hub || !ocrWorker || shuttingDown || !running)
        return;
    CameraSession *s = hub->session(camId);
    if (!s || s->lastMat.empty())
        return;

    s->triggerPending = false;
    emit detecting(camId);
    emit requestOcr(camId, s->lastMat.clone(), ocrWorker->currentEpoch());
}

void OcrScheduler::submitTriggered()
{
    if (!running || shuttingDown || !hub)
        return;
    for (int id = 1; id <= CameraHub::kCamCount; ++id) {
        CameraSession *s = hub->session(id);
        if (s && s->triggerPending && !s->lastMat.empty())
            submitOcr(id);
    }
}

void OcrScheduler::onOcrBusyChanged(bool busy)
{
    if (shuttingDown || !running)
        return;
    ocrBusy = busy;
    if (!busy)
        submitTriggered();
}

void OcrScheduler::onOcrResult(int camId, int code, QImage annotated)
{
    if (shuttingDown || !running || !hub)
        return;

    CameraSession *s = hub->session(camId);
    if (!s)
        return;

    s->lastAnnotated = annotated;
    s->holdAnnotated = true;
    s->annotatedHoldTimer.restart();
    if (code == 1)
        ++s->countOk;

    emit resultReady(camId, code, annotated);
}
