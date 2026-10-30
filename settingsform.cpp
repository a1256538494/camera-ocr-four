#include "settingsform.h"
#include "ui_settingsform.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

namespace {
constexpr auto kDefaultPlcIp = "127.0.0.1";
constexpr int kDefaultPlcPort = 502;
constexpr auto kDefaultStation = "产线A";
constexpr auto kDefaultLogPath = "D:/logs/modbusCamera";
constexpr auto kDefaultNgPath = "D:/data/ng";
}

settingsform::settingsform(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::settingsform)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);

    // 中文按钮文案（未加载 qt_zh_CN 时标准按钮可能仍是英文）
    if (QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok))
        ok->setText(QStringLiteral("确定"));
    if (QPushButton *cancel = ui->buttonBox->button(QDialogButtonBox::Cancel))
        cancel->setText(QStringLiteral("取消"));
    if (QPushButton *apply = ui->buttonBox->button(QDialogButtonBox::Apply))
        apply->setText(QStringLiteral("应用"));
    if (QPushButton *restore = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
        restore->setText(QStringLiteral("恢复默认"));

    // Modbus 映射页：只读（.ui 已设 readOnly；此处再禁用防止滚轮改值）
    ui->slaveIdSpin->setEnabled(false);
    ui->triggerAddrSpin->setEnabled(false);
    ui->pollMsSpin->setEnabled(false);

    if (ui->logPathEdit->text().trimmed().isEmpty())
        ui->logPathEdit->setText(QString::fromUtf8(kDefaultLogPath));
    if (ui->ngPathEdit->text().trimmed().isEmpty())
        ui->ngPathEdit->setText(QString::fromUtf8(kDefaultNgPath));

    captureBaseline();

    ui->navList->setCurrentRow(0);
    ui->pages->setCurrentIndex(0);
    connect(ui->navList, &QListWidget::currentRowChanged, this, &settingsform::onNavChanged);

    connect(ui->logPathBrowseBtn, &QPushButton::clicked, this, &settingsform::browseLogPath);
    connect(ui->ngPathBrowseBtn, &QPushButton::clicked, this, &settingsform::browseNgPath);
    connect(ui->cam1PathBrowseBtn, &QPushButton::clicked, this, [this]() { browseCamPath(1); });
    connect(ui->cam2PathBrowseBtn, &QPushButton::clicked, this, [this]() { browseCamPath(2); });
    connect(ui->cam3PathBrowseBtn, &QPushButton::clicked, this, [this]() { browseCamPath(3); });
    connect(ui->cam4PathBrowseBtn, &QPushButton::clicked, this, [this]() { browseCamPath(4); });

    // 不用 .ui 里 accepted→accept，以便先弹「日志目录重启生效」提示
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &settingsform::onButtonClicked);
}

settingsform::~settingsform()
{
    delete ui;
    ui = nullptr;
}

void settingsform::captureBaseline()
{
    baselineLogPath = ui->logPathEdit->text().trimmed();
}

bool settingsform::logPathChanged() const
{
    return ui->logPathEdit->text().trimmed() != baselineLogPath;
}

void settingsform::onNavChanged(int row)
{
    if (row >= 0 && row < ui->pages->count())
        ui->pages->setCurrentIndex(row);
}

void settingsform::browseLogPath()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择日志目录"),
                                                          ui->logPathEdit->text());
    if (!dir.isEmpty())
        ui->logPathEdit->setText(dir);
}

void settingsform::browseNgPath()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择 NG 图片目录"),
                                                          ui->ngPathEdit->text());
    if (!dir.isEmpty())
        ui->ngPathEdit->setText(dir);
}

void settingsform::browseCamPath(int camId)
{
    QLineEdit *edit = nullptr;
    switch (camId) {
    case 1: edit = ui->cam1PathEdit; break;
    case 2: edit = ui->cam2PathEdit; break;
    case 3: edit = ui->cam3PathEdit; break;
    case 4: edit = ui->cam4PathEdit; break;
    default: return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 Cam%1 视频").arg(camId, 2, 10, QChar('0')),
        edit->text(),
        QStringLiteral("视频 (*.mp4 *.avi *.mkv);;所有文件 (*.*)"));
    if (!path.isEmpty())
        edit->setText(path);
}

void settingsform::onButtonClicked(QAbstractButton *button)
{
    switch (ui->buttonBox->standardButton(button)) {
    case QDialogButtonBox::Ok:
        applySettings(true);
        break;
    case QDialogButtonBox::Apply:
        applySettings(false);
        break;
    case QDialogButtonBox::RestoreDefaults:
        applyDefaults();
        break;
    case QDialogButtonBox::Cancel:
        reject();
        break;
    default:
        break;
    }
}

void settingsform::applyDefaults()
{
    ui->plcIpEdit->setText(QString::fromUtf8(kDefaultPlcIp));
    ui->plcPortSpin->setValue(kDefaultPlcPort);
    ui->modbusEnabledCheck->setChecked(false);
    ui->logPathEdit->setText(QString::fromUtf8(kDefaultLogPath));
    ui->ngPathEdit->setText(QString::fromUtf8(kDefaultNgPath));
    ui->saveNgCheck->setChecked(true);
    ui->stationNameEdit->setText(QString::fromUtf8(kDefaultStation));
    ui->layoutCombo->setCurrentIndex(0);
    // 映射页只读，保持约定值展示
    ui->slaveIdSpin->setValue(1);
    ui->triggerAddrSpin->setValue(101);
    ui->pollMsSpin->setValue(100);
}

void settingsform::notifyRestartIfNeeded()
{
    if (!logPathChanged())
        return;

    QMessageBox::information(
        this,
        QStringLiteral("需要重启"),
        QStringLiteral("日志目录已修改，需重启应用程序后生效。\n\n"
                       "当前会话仍使用原日志目录；其他已保存项可按规则立即或下次「全部开始」生效。"));
}

bool settingsform::applySettings(bool closeAfter)
{
    // TODO: ConfigService::save(...)；当前先落 UI 行为与重启提示
    const bool needRestartHint = logPathChanged();

    // 成功写入后，把 baseline 更新为当前值，避免重复弹窗（应用后再确定）
    // 但若本次改了日志目录，仍应提示一次
    if (needRestartHint)
        notifyRestartIfNeeded();

    captureBaseline();

    if (closeAfter)
        accept();
    return true;
}
