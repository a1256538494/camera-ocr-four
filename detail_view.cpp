#include "detail_view.h"
#include "ui_detail_view.h"

#include <QPixmap>
#include <QPushButton>

DetailView::DetailView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DetailView)
{
    ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);
    connect(ui->backbtn, &QPushButton::clicked, this, &DetailView::backRequested);
}

DetailView::~DetailView()
{
    delete ui;
    ui = nullptr;
}

void DetailView::bindCamera(int id, const QString &title)
{
    camId = id;
    ui->titleLabel->setText(title);
    setWindowTitle(QStringLiteral("仔细查看 - %1").arg(title));
    currentImage = QImage();
    ui->imageLabel->setPixmap(QPixmap());
    ui->imageLabel->setText(QStringLiteral("等待 Cam%1 画面…").arg(id, 2, 10, QChar('0')));
}

void DetailView::updateFrame(int id, const QImage &image)
{
    if (id != camId || image.isNull())
        return;
    currentImage = image;
    applyPixmap();
}

void DetailView::updateState(int id, const QString &stateText)
{
    if (id != camId)
        return;
    ui->stateLabel->setText(stateText.startsWith(QStringLiteral("状态"))
                                ? stateText
                                : QStringLiteral("状态: %1").arg(stateText));
}

void DetailView::applyPixmap()
{
    if (currentImage.isNull() || !ui || !ui->imageLabel)
        return;
    const QSize target = ui->imageLabel->size();
    if (target.width() < 2 || target.height() < 2)
        return;
    ui->imageLabel->setPixmap(QPixmap::fromImage(currentImage).scaled(
        target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void DetailView::closeEvent(QCloseEvent *event)
{
    emit backRequested();
    event->ignore(); // 由主界面 hide，避免销毁
}

void DetailView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit backRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void DetailView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyPixmap();
}
