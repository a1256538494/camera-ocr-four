#ifndef CAMERA_TILE_VIEW_H
#define CAMERA_TILE_VIEW_H

#include <QFrame>
#include <QImage>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class CameraTileView; }
QT_END_NAMESPACE

// 单路宫格单元：自带 camera_tile.ui；不采流，只展示/点击
class CameraTileView : public QFrame
{
    Q_OBJECT
public:
    explicit CameraTileView(int camId, const QString &title, QWidget *parent = nullptr);
    ~CameraTileView() override;

    int camId() const { return id; }
    QString titleText() const;
    void setTitle(const QString &title);
    void setState(const QString &stateText);
    void setFps(double fps);
    void setImage(const QImage &image);
    void clearImage();

signals:
    void clicked(int camId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void prepareInteraction();

    Ui::CameraTileView *ui = nullptr;
    int id = 0;
};

#endif // CAMERA_TILE_VIEW_H
