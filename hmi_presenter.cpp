#include "hmi_presenter.h"
#include "camera_hub.h"
#include "camera_tile_view.h"
#include "detail_navigator.h"

#include <QLabel>
#include <QListWidget>
#include <QDateTime>

HmiPresenter::HmiPresenter(QLabel *systemStatus,
                           QLabel *station,
                           QLabel *clock,
                           QLabel *online,
                           QLabel *ok,
                           QLabel *ng,
                           QLabel *alarm,
                           QLabel *modbus,
                           QLabel *upload,
                           QLabel *resource,
                           QListWidget *events,
                           CameraTileView *t1,
                           CameraTileView *t2,
                           CameraTileView *t3,
                           CameraTileView *t4,
                           CameraHub *cameraHub,
                           QObject *parent)
    : QObject(parent)
    , systemStatusLabel(systemStatus)
    , stationLabel(station)
    , clockLabel(clock)
    , onlineValueLabel(online)
    , okValueLabel(ok)
    , ngValueLabel(ng)
    , alarmValueLabel(alarm)
    , modbusStatusLabel(modbus)
    , uploadStatusLabel(upload)
    , resourceLabel(resource)
    , eventsList(events)
    , tile1(t1)
    , tile2(t2)
    , tile3(t3)
    , tile4(t4)
    , hub(cameraHub)
{
}

CameraTileView *HmiPresenter::tileAt(int camId) const
{
    if (camId == 1)
        return tile1;
    if (camId == 2)
        return tile2;
    if (camId == 3)
        return tile3;
    if (camId == 4)
        return tile4;
    return nullptr;
}

void HmiPresenter::setDetailNavigator(DetailNavigator *nav)
{
    detailNav = nav;
}

void HmiPresenter::setSystemStatus(const QString &text)
{
    if (systemStatusLabel)
        systemStatusLabel->setText(QStringLiteral("总状态: %1").arg(text));
}

void HmiPresenter::setStationName(const QString &name)
{
    if (stationLabel)
        stationLabel->setText(QStringLiteral("工位: %1").arg(name));
}

void HmiPresenter::setClockText(const QString &text)
{
    if (clockLabel)
        clockLabel->setText(text);
}

void HmiPresenter::setCamState(int camId, const QString &stateText)
{
    const QString text = stateText.startsWith(QStringLiteral("状态"))
                             ? stateText
                             : QStringLiteral("状态: %1").arg(stateText);
    if (hub) {
        if (CameraSession *s = hub->session(camId))
            s->stateText = text;
    }
    if (CameraTileView *t = tileAt(camId))
        t->setState(text);
    if (detailNav)
        detailNav->updateStateIfCurrent(camId, text);
}

void HmiPresenter::setCamFps(int camId, double fps)
{
    if (hub) {
        if (CameraSession *s = hub->session(camId))
            s->fps = fps;
    }
    if (CameraTileView *t = tileAt(camId))
        t->setFps(fps);
}

void HmiPresenter::setOnlineCount(int online, int total)
{
    if (onlineValueLabel)
        onlineValueLabel->setText(QStringLiteral("%1 / %2").arg(online).arg(total));
}

void HmiPresenter::setOkCount(int count)
{
    okTotal = count;
    if (okValueLabel)
        okValueLabel->setText(QString::number(okTotal));
}

void HmiPresenter::setNgCount(int count)
{
    ngTotal = count;
    if (ngValueLabel)
        ngValueLabel->setText(QString::number(ngTotal));
}

void HmiPresenter::setAlarmText(const QString &text)
{
    if (alarmValueLabel)
        alarmValueLabel->setText(text);
}

void HmiPresenter::setModbusStatus(const QString &text)
{
    if (modbusStatusLabel)
        modbusStatusLabel->setText(QStringLiteral("Modbus: %1").arg(text));
}

void HmiPresenter::setUploadStatus(const QString &text)
{
    if (uploadStatusLabel)
        uploadStatusLabel->setText(QStringLiteral("上传: %1").arg(text));
}

void HmiPresenter::setResourceText(const QString &text)
{
    if (resourceLabel)
        resourceLabel->setText(text);
}

void HmiPresenter::pushEvent(const QString &text)
{
    if (!eventsList)
        return;
    eventsList->insertItem(0, text);
    while (eventsList->count() > 100)
        delete eventsList->takeItem(eventsList->count() - 1);
}

void HmiPresenter::refreshTiles()
{
    if (!hub)
        return;
    for (int id = 1; id <= 4; ++id) {
        CameraTileView *t = tileAt(id);
        if (!t)
            continue;
        const QImage img = hub->displayImage(id);
        if (!img.isNull())
            t->setImage(img);
    }
}

void HmiPresenter::clearTiles()
{
    for (int id = 1; id <= 4; ++id) {
        CameraTileView *t = tileAt(id);
        if (!t)
            continue;
        t->clearImage();
        t->setFps(0.0);
        t->setState(QStringLiteral("空闲"));
        if (hub) {
            if (CameraSession *s = hub->session(id))
                s->fps = 0.0;
        }
    }
}

void HmiPresenter::onOcrResult(int camId, int code)
{
    QString state;
    switch (code) {
    case 1:
        state = QStringLiteral("OK");
        setOkCount(okTotal + 1);
        break;
    case 2:
        state = QStringLiteral("NG");
        setNgCount(ngTotal + 1);
        break;
    default:
        state = QStringLiteral("未检出");
        break;
    }
    setCamState(camId, state);
    refreshTiles();
    if (detailNav)
        detailNav->notifyFrame(camId);

    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    pushEvent(QStringLiteral("%1 Cam%2 %3 (code=%4)")
                  .arg(ts)
                  .arg(camId, 2, 10, QChar('0'))
                  .arg(state)
                  .arg(code));
}
