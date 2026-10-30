#ifndef DETAIL_NAVIGATOR_H
#define DETAIL_NAVIGATOR_H

#include <QObject>
#include <QString>

class QWidget;
class CameraHub;
class CameraTileView;
class DetailView;
class HmiPresenter;

class DetailNavigator : public QObject
{
    Q_OBJECT
public:
    DetailNavigator(QWidget *overview,
                    CameraHub *hub,
                    CameraTileView *tile1,
                    CameraTileView *tile2,
                    CameraTileView *tile3,
                    CameraTileView *tile4,
                    QObject *parent = nullptr);
    ~DetailNavigator() override;

    void setPresenter(HmiPresenter *p);
    void open(int camId);
    void closeDetail();
    void destroyView();
    void notifyFrame(int camId);
    void updateStateIfCurrent(int camId, const QString &stateText);
    int currentCamId() const { return detailCameraId; }

private slots:
    void onBack();

private:
    QString titleFor(int camId) const;

    QWidget *overview = nullptr;
    CameraHub *hub = nullptr;
    CameraTileView *tile1 = nullptr;
    CameraTileView *tile2 = nullptr;
    CameraTileView *tile3 = nullptr;
    CameraTileView *tile4 = nullptr;
    HmiPresenter *presenter = nullptr;
    DetailView *detailView = nullptr;
    int detailCameraId = 0;
};

#endif // DETAIL_NAVIGATOR_H
