#include "detail_navigator.h"
#include "detail_view.h"
#include "camera_hub.h"
#include "camera_tile_view.h"
#include "hmi_presenter.h"

#include <QWidget>

DetailNavigator::DetailNavigator(QWidget *overviewWidget,
                                 CameraHub *cameraHub,
                                 CameraTileView *t1,
                                 CameraTileView *t2,
                                 CameraTileView *t3,
                                 CameraTileView *t4,
                                 QObject *parent)
    : QObject(parent)
    , overview(overviewWidget)
    , hub(cameraHub)
    , tile1(t1)
    , tile2(t2)
    , tile3(t3)
    , tile4(t4)
{
    detailView = new DetailView(nullptr);
    detailView->hide();
    connect(detailView, &DetailView::backRequested, this, &DetailNavigator::onBack);
}

DetailNavigator::~DetailNavigator()
{
    destroyView();
}

QString DetailNavigator::titleFor(int camId) const
{
    CameraTileView *t = nullptr;
    if (camId == 1)
        t = tile1;
    else if (camId == 2)
        t = tile2;
    else if (camId == 3)
        t = tile3;
    else if (camId == 4)
        t = tile4;
    if (t)
        return t->titleText();
    return QStringLiteral("Cam%1").arg(camId, 2, 10, QChar('0'));
}

void DetailNavigator::setPresenter(HmiPresenter *p)
{
    presenter = p;
}

void DetailNavigator::destroyView()
{
    if (!detailView)
        return;
    disconnect(detailView, nullptr, this, nullptr);
    detailView->hide();
    delete detailView;
    detailView = nullptr;
    detailCameraId = 0;
}

void DetailNavigator::open(int camId)
{
    if (!detailView || !overview || !hub || camId < 1)
        return;

    detailCameraId = camId;
    detailView->bindCamera(camId, titleFor(camId));
    notifyFrame(camId);

    overview->hide();
    detailView->show();
    detailView->raise();
    detailView->activateWindow();

    if (presenter) {
        presenter->pushEvent(QStringLiteral("仔细查看 Cam%1（主界面已隐藏，后台继续）")
                                 .arg(camId, 2, 10, QChar('0')));
    }
}

void DetailNavigator::closeDetail()
{
    if (detailCameraId == 0)
        return;
    const int left = detailCameraId;
    detailCameraId = 0;
    if (detailView)
        detailView->hide();
    if (overview) {
        overview->show();
        overview->raise();
        overview->activateWindow();
    }
    if (presenter) {
        presenter->pushEvent(QStringLiteral("返回主界面 (离开 Cam%1)")
                                 .arg(left, 2, 10, QChar('0')));
    }
}

void DetailNavigator::onBack()
{
    closeDetail();
}

void DetailNavigator::notifyFrame(int camId)
{
    if (!detailView || detailCameraId != camId || !hub)
        return;
    const QImage img = hub->displayImage(camId);
    if (!img.isNull())
        detailView->updateFrame(camId, img);
    if (const CameraSession *s = hub->session(camId)) {
        detailView->updateState(camId, s->stateText.startsWith(QStringLiteral("状态"))
                                           ? s->stateText
                                           : QStringLiteral("状态: %1").arg(s->stateText));
    }
}

void DetailNavigator::updateStateIfCurrent(int camId, const QString &stateText)
{
    if (detailView && detailCameraId == camId)
        detailView->updateState(camId, stateText);
}
