#ifndef MAIN_INTERFACE_H
#define MAIN_INTERFACE_H

#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QResizeEvent>
#include <QCloseEvent>

class CameraHub;
class OcrScheduler;
class CameraTileView;
class HmiPresenter;
class DetailNavigator;

QT_BEGIN_NAMESPACE
namespace Ui { class main_interface; }
QT_END_NAMESPACE

// 主壳：编排与生命周期。宫格运行时装入 4×CameraTileView（camera_tile.ui）；
// UI→HmiPresenter；详情→DetailNavigator。本地视频 + 100ms 模拟 OCR，暂不连 Modbus。
class main_interface : public QWidget
{
    Q_OBJECT

public:
    explicit main_interface(QWidget *parent = nullptr);
    ~main_interface();

signals:
    void startAllRequested();
    void stopAllRequested();
    void settingRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onClockTick();
    void onCaptureTick();
    void onUiTick();
    void onStartAll();
    void onStopAll();
    void onOpenSettings();
    void onTileClicked(int camId);
    void onHubCamOpened(int camId, const QString &path);
    void onHubCamOpenFailed(int camId, const QString &path);
    void onHubFrameCaptured(int camId);
    void onHubFpsHint(int camId, double fps);
    void onOcrDetecting(int camId);
    void onOcrResult(int camId, int code, QImage annotated);
    void onOcrStatusText(const QString &text);

private:
    void setupUiExtras();
    void setupServices();
    void stopRuntimeOnly(); // 停 Timer/视频，不阻塞等 OCR
    void onOcrStopped();

    Ui::main_interface *ui = nullptr;

    CameraHub *cameraHub = nullptr;
    OcrScheduler *ocrScheduler = nullptr;
    CameraTileView *tile1 = nullptr;
    CameraTileView *tile2 = nullptr;
    CameraTileView *tile3 = nullptr;
    CameraTileView *tile4 = nullptr;
    HmiPresenter *presenter = nullptr;
    DetailNavigator *detailNav = nullptr;

    QTimer *clockTimer = nullptr;
    QTimer *captureTimer = nullptr;
    QTimer *uiTimer = nullptr;

    bool running = false;
    bool shuttingDown = false;
};

#endif // MAIN_INTERFACE_H
