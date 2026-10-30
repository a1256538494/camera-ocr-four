#ifndef OCR_SCHEDULER_H
#define OCR_SCHEDULER_H

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QThread>

#include <opencv2/core.hpp>

class CameraHub;
class OcrWorker;

// OCR 调度：本地阶段 100ms 模拟 PLC 触发。退出用异步停止，禁止 terminate 打断 NCNN。
class OcrScheduler : public QObject
{
    Q_OBJECT
public:
    static constexpr int kTriggerIntervalMs = 100;

    explicit OcrScheduler(CameraHub *hub, QObject *parent = nullptr);
    ~OcrScheduler() override;

    void start();
    void stop();
    void requestStopAsync(); // 关窗：不阻塞 UI，结束后发 stopped()
    void shutdownSync();     // 析构兜底：最长等待，绝不 terminate

signals:
    void requestOcr(int camId, cv::Mat frame, int epoch);
    void detecting(int camId);
    void resultReady(int camId, int code, QImage annotated);
    void statusText(const QString &text);
    void stopped();

private slots:
    void onTriggerPulse();
    void onOcrResult(int camId, int code, QImage annotated);
    void onOcrBusyChanged(bool busy);
    void onWorkerThreadFinished();

private:
    void setupWorker();
    void submitOcr(int camId);
    void submitTriggered();
    void beginStop();

    CameraHub *hub = nullptr;
    QTimer *triggerTimer = nullptr;
    QThread *ocrThread = nullptr;
    OcrWorker *ocrWorker = nullptr;
    bool ocrBusy = false;
    bool running = false;
    bool shuttingDown = false;
    bool stopNotified = false;
};

#endif // OCR_SCHEDULER_H
