#include "camera_tile_view.h"
#include "ui_camera_tile.h"

#include <QLabel>
#include <QMouseEvent>
#include <QCursor>
#include <QPixmap>
#include <QSizePolicy>

CameraTileView::CameraTileView(int camId, const QString &title, QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::CameraTileView)
    , id(camId)
{
    ui->setupUi(this);
    setTitle(title.isEmpty()
                 ? QStringLiteral("Cam%1").arg(camId, 2, 10, QChar('0'))
                 : title);
    prepareInteraction();
}

CameraTileView::~CameraTileView()
{
    delete ui;
    ui = nullptr;
}

void CameraTileView::prepareInteraction()
{
    setCursor(Qt::PointingHandCursor);
    installEventFilter(this);

    if (ui->imageLabel) {
        ui->imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->imageLabel->setMinimumSize(0, 0);
        ui->imageLabel->setAlignment(Qt::AlignCenter);
        ui->imageLabel->setScaledContents(false);
    }

    const auto children = findChildren<QWidget *>();
    for (QWidget *child : children) {
        if (child != this)
            child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
}

QString CameraTileView::titleText() const
{
    return ui && ui->titleLabel ? ui->titleLabel->text()
                                : QStringLiteral("Cam%1").arg(id, 2, 10, QChar('0'));
}

void CameraTileView::setTitle(const QString &title)
{
    if (ui && ui->titleLabel)
        ui->titleLabel->setText(title);
}

void CameraTileView::setState(const QString &stateText)
{
    if (!ui || !ui->stateLabel)
        return;
    if (stateText.startsWith(QStringLiteral("状态")))
        ui->stateLabel->setText(stateText);
    else
        ui->stateLabel->setText(QStringLiteral("状态: %1").arg(stateText));
}

void CameraTileView::setFps(double fps)
{
    if (ui && ui->fpsLabel)
        ui->fpsLabel->setText(QStringLiteral("FPS: %1").arg(fps, 0, 'f', 1));
}

void CameraTileView::clearImage()
{
    if (ui && ui->imageLabel)
        ui->imageLabel->clear();
}

void CameraTileView::setImage(const QImage &image)
{
    if (!ui || !ui->imageLabel || image.isNull())
        return;
    const QSize target = ui->imageLabel->size();
    if (target.width() < 2 || target.height() < 2)
        return;
    ui->imageLabel->setPixmap(QPixmap::fromImage(image).scaled(
        target, Qt::KeepAspectRatio, Qt::FastTransformation));
}

bool CameraTileView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            emit clicked(id);
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}
