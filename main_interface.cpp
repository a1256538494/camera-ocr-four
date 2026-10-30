#include "main_interface.h"
#include "ui_main_interface.h"
#include "camera_hub.h"
#include "ocr_scheduler.h"
#include "camera_tile_view.h"
#include "hmi_presenter.h"
#include "detail_navigator.h"
#include "settingsform.h"

#include <QPushButton>
#include <QGridLayout>
#include <QDateTime>
#include <QApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QLayoutItem>

namespace {
constexpr int kUiIntervalMs = 200;
}

main_interface::main_interface(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::main_interface)
{
    ui->setupUi(this);
    setupUiExtras();
    setupServices();

    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &main_interface::onClockTick);
    clockTimer->start(1000);
    onClockTick();

    captureTimer = new QTimer(this);
    connect(captureTimer, &QTimer::timeout, this, &main_interface::onCaptureTick);

    uiTimer = new QTimer(this);
    connect(uiTimer, &QTimer::timeout, this, &main_interface::onUiTick);

    connect(ui->startAllbtn, &QPushButton::clicked, this, &main_interface::onStartAll);
    connect(ui->stopAllbtn, &QPushButton::clicked, this, &main_interface::onStopAll);
    connect(ui->settingsbtn, &QPushButton::clicked, this, &main_interface::onOpenSettings);
    connect(this, &main_interface::startAllRequested, this, &main_interface::onStartAll);
    connect(this, &main_interface::stopAllRequested, this, &main_interface::onStopAll);
    connect(this, &main_interface::settingRequested, this, &main_interface::onOpenSettings);

    presenter->setSystemStatus(QStringLiteral("空闲"));
    for (int i = 1; i <= CameraHub::kCamCount; ++i)
        presenter->setCamState(i, QStringLiteral("空闲"));
    presenter->setOnlineCount(0, CameraHub::kCamCount);
    presenter->setOkCount(0);
    presenter->setNgCount(0);
    presenter->setModbusStatus(QStringLiteral("● 未启用（本地视频）"));
}

main_interface::~main_interface(){
    stopRuntimeOnly();
    if (ocrScheduler)
        ocrScheduler->shutdownSync();
    if (detailNav)
        detailNav->destroyView();
    delete ui;
    ui = nullptr;
}
// 壳布局 + 运行时装入 camera_tile.ui 实例（宫格不再内嵌四套 XML）
void main_interface::setupUiExtras()
{
    ui->cameraGridLayout->setRowStretch(0, 1);
    ui->cameraGridLayout->setRowStretch(1, 1);
    ui->cameraGridLayout->setColumnStretch(0, 1);
    ui->cameraGridLayout->setColumnStretch(1, 1);
    ui->mainSplitter->setStretchFactor(0, 3);
    ui->mainSplitter->setStretchFactor(1, 1);

    // 若误包含旧版 ui_main_interface.h，格子里可能已有 tileCam*；
    // addWidget 同格只会「挤出布局」不删除，旧控件仍可见 → 重影。装载前清空。
    while (QLayoutItem *item = ui->cameraGridLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    static const QString kTitles[4] = {
        QStringLiteral("Cam01 | 工位A-喷码"),
        QStringLiteral("Cam02 | 工位B-喷码"),
        QStringLiteral("Cam03 | 工位C-喷码"),
        QStringLiteral("Cam04 | 工位D-喷码"),
    };

    tile1 = new CameraTileView(1, kTitles[0], ui->gridHost);
    tile2 = new CameraTileView(2, kTitles[1], ui->gridHost);
    tile3 = new CameraTileView(3, kTitles[2], ui->gridHost);
    tile4 = new CameraTileView(4, kTitles[3], ui->gridHost);

    // 固定 2×2，不提供布局切换
    ui->cameraGridLayout->addWidget(tile1, 0, 0);
    ui->cameraGridLayout->addWidget(tile2, 0, 1);
    ui->cameraGridLayout->addWidget(tile3, 1, 0);
    ui->cameraGridLayout->addWidget(tile4, 1, 1);

    connect(tile1, &CameraTileView::clicked, this, &main_interface::onTileClicked);
    connect(tile2, &CameraTileView::clicked, this, &main_interface::onTileClicked);
    connect(tile3, &CameraTileView::clicked, this, &main_interface::onTileClicked);
    connect(tile4, &CameraTileView::clicked, this, &main_interface::onTileClicked);
}
//设置camera的service，业务部分
void main_interface::setupServices(){
    cameraHub = new CameraHub(this);
    cameraHub->setCamTitle(1, tile1->titleText());
    cameraHub->setCamTitle(2, tile2->titleText());
    cameraHub->setCamTitle(3, tile3->titleText());
    cameraHub->setCamTitle(4, tile4->titleText());

    ocrScheduler = new OcrScheduler(cameraHub, this);

    presenter = new HmiPresenter(ui->systemStatusLabel, ui->stationLabel, ui->clockLabel,
                                 ui->onlineValueLabel, ui->okValueLabel, ui->ngValueLabel,
                                 ui->alarmValueLabel, ui->modbusStatusLabel, ui->uploadStatusLabel,
                                 ui->resourceLabel, ui->eventsList,
                                 tile1, tile2, tile3, tile4, cameraHub, this);

    detailNav = new DetailNavigator(this, cameraHub, tile1, tile2, tile3, tile4, this);
    presenter->setDetailNavigator(detailNav);
    detailNav->setPresenter(presenter);

    connect(cameraHub, &CameraHub::camOpened, this, &main_interface::onHubCamOpened);
    connect(cameraHub, &CameraHub::camOpenFailed, this, &main_interface::onHubCamOpenFailed);
    connect(cameraHub, &CameraHub::frameCaptured, this, &main_interface::onHubFrameCaptured);
    connect(cameraHub, &CameraHub::fpsHint, this, &main_interface::onHubFpsHint);

    connect(ocrScheduler, &OcrScheduler::detecting, this, &main_interface::onOcrDetecting);
    connect(ocrScheduler, &OcrScheduler::resultReady, this, &main_interface::onOcrResult);
    connect(ocrScheduler, &OcrScheduler::statusText, this, &main_interface::onOcrStatusText);
    connect(ocrScheduler, &OcrScheduler::stopped, this, &main_interface::onOcrStopped);
}

void main_interface::closeEvent(QCloseEvent *event)
{
    // 关窗不能在 UI 线程 wait OCR，否则未响应；也禁止 terminate NCNN 线程
    shuttingDown = true;
    if (clockTimer)
        clockTimer->stop();
    stopRuntimeOnly();
    if (detailNav)
        detailNav->destroyView();
    hide();

    if (ocrScheduler) {
        ocrScheduler->requestStopAsync();
        // 兜底：当前帧 OCR 结束后会 quit；最长 15s 仍强制退进程
        QTimer::singleShot(15000, qApp, &QApplication::quit);
    } else {
        QApplication::quit();
    }

    event->accept();
}

void main_interface::onOcrStopped()
{
    QApplication::quit();
}

void main_interface::stopRuntimeOnly()
{
    shuttingDown = true;
    running = false;
    if (captureTimer)
        captureTimer->stop();
    if (uiTimer)
        uiTimer->stop();
    // 软停保留 clockTimer，关窗由 closeEvent/析构收尾
    if (ocrScheduler)
        ocrScheduler->stop();
    if (cameraHub)
        cameraHub->closeAll();
}

void main_interface::onStartAll(){
    if (running || !cameraHub || !ocrScheduler || !presenter)
        return;

    shuttingDown = false;
    if (!cameraHub->openAll()) {
        presenter->setSystemStatus(QStringLiteral("故障"));
        presenter->setAlarmText(QStringLiteral("全部视频打开失败"));
        return;
    }

    running = true;
    captureTimer->start(CameraHub::kCaptureIntervalMs);
    uiTimer->start(kUiIntervalMs);
    ocrScheduler->start();

    const int online = cameraHub->openedCount();
    presenter->setOnlineCount(online, CameraHub::kCamCount);
    presenter->setAlarmText(online < CameraHub::kCamCount
                                ? QStringLiteral("部分路打开失败 (%1/%2)").arg(online).arg(CameraHub::kCamCount)
                                : QStringLiteral("无"));
    for (int i = 1; i <= CameraHub::kCamCount; ++i) {
        if (cameraHub->isOpened(i))
            presenter->setCamState(i, QStringLiteral("预览中"));
    }
    presenter->setSystemStatus(QStringLiteral("运行中"));
    presenter->setModbusStatus(QStringLiteral("● 未启用（本地视频）"));
    ui->startAllbtn->setEnabled(false);
    ui->stopAllbtn->setEnabled(true);
    presenter->pushEvent(QStringLiteral("全部开始：%1 路本地视频；OCR 节拍 %2 ms")
                             .arg(online)
                             .arg(OcrScheduler::kTriggerIntervalMs));
}

void main_interface::onOpenSettings()
{
    if (shuttingDown)
        return;
    settingsform dlg(this); // ApplicationModal：主界面不隐藏，但不可点
    dlg.exec();
}

void main_interface::onStopAll()
{
    if (shuttingDown || !running)
        return;

    stopRuntimeOnly();
    shuttingDown = false; // 允许再次「全部开始」；OCR 侧已 abort+断连，结果靠 !running 丢弃

    if (presenter) {
        presenter->clearTiles();
        presenter->setSystemStatus(QStringLiteral("空闲"));
        presenter->setOnlineCount(0, CameraHub::kCamCount);
        presenter->pushEvent(QStringLiteral("已停止"));
    }
    ui->startAllbtn->setEnabled(true);
    ui->stopAllbtn->setEnabled(true);
}

void main_interface::onCaptureTick()
{
    if (running && cameraHub)
        cameraHub->captureTick();
}

void main_interface::onUiTick()
{
    if (running && presenter)
        presenter->refreshTiles();
}

void main_interface::onClockTick()
{
    if (presenter) {
        presenter->setClockText(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    }
}

void main_interface::onTileClicked(int camId)
{
    if (detailNav)
        detailNav->open(camId);
}

void main_interface::onHubCamOpened(int camId, const QString &path)
{
    if (presenter) {
        presenter->pushEvent(QStringLiteral("Cam%1 已打开: %2")
                                 .arg(camId, 2, 10, QChar('0'))
                                 .arg(path));
    }
}

void main_interface::onHubCamOpenFailed(int camId, const QString &path)
{
    if (presenter) {
        presenter->setCamState(camId, QStringLiteral("故障"));
        presenter->pushEvent(QStringLiteral("Cam%1 打开失败: %2")
                                 .arg(camId, 2, 10, QChar('0'))
                                 .arg(path));
    }
}

void main_interface::onHubFrameCaptured(int camId)
{
    if (detailNav)
        detailNav->notifyFrame(camId);
}

void main_interface::onHubFpsHint(int camId, double fps)
{
    if (presenter)
        presenter->setCamFps(camId, fps);
}

void main_interface::onOcrDetecting(int camId)
{
    if (shuttingDown || !running || !presenter)
        return;
    presenter->setCamState(camId, QStringLiteral("检测中"));
    if (detailNav)
        detailNav->notifyFrame(camId);
}

void main_interface::onOcrResult(int camId, int code, QImage annotated)
{
    Q_UNUSED(annotated);
    if (shuttingDown || !running || !presenter)
        return;
    presenter->onOcrResult(camId, code);
}

void main_interface::onOcrStatusText(const QString &text)
{
    if (presenter)
        presenter->pushEvent(text);
}

void main_interface::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (presenter)
        presenter->refreshTiles();
}


