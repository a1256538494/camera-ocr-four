#ifndef SETTINGSFORM_H
#define SETTINGSFORM_H

#include <QDialog>
#include <QString>

class QAbstractButton;

QT_BEGIN_NAMESPACE
namespace Ui { class settingsform; }
QT_END_NAMESPACE

// 应用级模态设置对话框：不隐藏主界面，但主界面不可点（类似系统环境变量窗口）
class settingsform : public QDialog
{
    Q_OBJECT

public:
    explicit settingsform(QWidget *parent = nullptr);
    ~settingsform() override;

private slots:
    void onNavChanged(int row);
    void browseLogPath();
    void browseNgPath();
    void browseCamPath(int camId);
    void onButtonClicked(QAbstractButton *button);

private:
    void captureBaseline();
    void applyDefaults();
    bool applySettings(bool closeAfter);
    void notifyRestartIfNeeded();
    bool logPathChanged() const;

    Ui::settingsform *ui = nullptr;
    QString baselineLogPath;
};

#endif // SETTINGSFORM_H
