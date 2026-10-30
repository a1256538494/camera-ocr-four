#ifndef OCR_WORKER_H
#define OCR_WORKER_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QHash>
#include <atomic>
#include <memory>
#include <opencv2/core.hpp>

class ocrDetect;

Q_DECLARE_METATYPE(cv::Mat)

// 工作线程 OCR。忙时按 camId 只留最新帧；abort 后不再 drain，便于安全退出。
class OcrWorker : public QObject
{
    Q_OBJECT
public:
    explicit OcrWorker(QObject *parent = nullptr);
    ~OcrWorker() override;

public slots:
    void processFrame(int camId, cv::Mat frame, int epoch);
    void clearPending();
    void requestAbort(); // 可跨线程：bump epoch + atomic + 清队列
    void resetAbort();   // 再次 start 前调用（不回退 epoch）

    int currentEpoch() const { return workEpoch.load(); }

signals:
    void resultReady(int camId, int code, QImage annotated);
    void busyChanged(bool busy);

private:
    void ensureEngine();
    void runDetect(int camId, cv::Mat frame, int epoch);
    void drainPending(int epoch);
    bool epochValid(int epoch) const;

    std::unique_ptr<ocrDetect> ocrEng;
    QMutex mutex;
    QHash<int, cv::Mat> pendingFrames;
    bool busy = false;
    std::atomic_bool abortFlag{false};
    std::atomic_int workEpoch{0}; // stop/abort 时 +1，已入队旧任务带旧 epoch 会被丢弃
};

#endif // OCR_WORKER_H
