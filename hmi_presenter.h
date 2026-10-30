#ifndef HMI_PRESENTER_H
#define HMI_PRESENTER_H

#include <QObject>
#include <QString>
#include <QImage>

class QLabel;
class QListWidget;
class CameraHub;
class CameraTileView;
class DetailNavigator;

class HmiPresenter : public QObject
{
    Q_OBJECT
public:
    HmiPresenter(QLabel *systemStatus,
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
                 CameraTileView *tile1,
                 CameraTileView *tile2,
                 CameraTileView *tile3,
                 CameraTileView *tile4,
                 CameraHub *hub,
                 QObject *parent = nullptr);

    void setDetailNavigator(DetailNavigator *nav);

    void setSystemStatus(const QString &text);
    void setStationName(const QString &name);
    void setClockText(const QString &text);
    void setCamState(int camId, const QString &stateText);
    void setCamFps(int camId, double fps);
    void setOnlineCount(int online, int total);
    void setOkCount(int count);
    void setNgCount(int count);
    void setAlarmText(const QString &text);
    void setModbusStatus(const QString &text);
    void setUploadStatus(const QString &text);
    void setResourceText(const QString &text);
    void pushEvent(const QString &text);
    void refreshTiles();
    void clearTiles();
    void onOcrResult(int camId, int code);

    int okCount() const { return okTotal; }
    int ngCount() const { return ngTotal; }

private:
    CameraTileView *tileAt(int camId) const;

    QLabel *systemStatusLabel = nullptr;
    QLabel *stationLabel = nullptr;
    QLabel *clockLabel = nullptr;
    QLabel *onlineValueLabel = nullptr;
    QLabel *okValueLabel = nullptr;
    QLabel *ngValueLabel = nullptr;
    QLabel *alarmValueLabel = nullptr;
    QLabel *modbusStatusLabel = nullptr;
    QLabel *uploadStatusLabel = nullptr;
    QLabel *resourceLabel = nullptr;
    QListWidget *eventsList = nullptr;
    CameraTileView *tile1 = nullptr;
    CameraTileView *tile2 = nullptr;
    CameraTileView *tile3 = nullptr;
    CameraTileView *tile4 = nullptr;
    CameraHub *hub = nullptr;
    DetailNavigator *detailNav = nullptr;
    int okTotal = 0;
    int ngTotal = 0;
};

#endif // HMI_PRESENTER_H
