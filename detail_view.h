#ifndef DETAIL_VIEW_H
#define DETAIL_VIEW_H

#include <QWidget>
#include <QImage>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QResizeEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class DetailView; }
QT_END_NAMESPACE

// 单路仔细查看窗：不打开相机/视频，只显示主界面某路 Session 推送的画面
class DetailView : public QWidget
{
    Q_OBJECT
public:
    explicit DetailView(QWidget *parent = nullptr);
    ~DetailView() override;

    void bindCamera(int camId, const QString &title);
    int boundCameraId() const { return camId; }

public slots:
    void updateFrame(int camId, const QImage &image);
    void updateState(int camId, const QString &stateText);

signals:
    void backRequested();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyPixmap();

    Ui::DetailView *ui = nullptr;
    int camId = 0;
    QImage currentImage;
};

#endif // DETAIL_VIEW_H
