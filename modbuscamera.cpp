#include "modbuscamera.h"
#include "ui_modbuscamera.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

bool modbusCamera::g_captureSignal = false;
bool modbusCamera::g_isExitThread = false;  // 控制线程退出
modbusCamera* modbusCamera::instance = nullptr;  // 初始化静态实例指针
int modbusCamera::send_index = 0;
int modbusCamera::count_ok = 0;

modbusCamera::modbusCamera(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::modbusCamera)
    , client(new QModbusTcpClient(this))
    , timer(new QTimer(this))
{
#ifdef NO_HUARAY_CAMERA
    useLocalVideo = true;
    autoTriggerDetect = true;
#endif

    ui->setupUi(this);
    instance = this;   // 将当前类实例保存为静态实例

    // 确保连接信号与槽
    connect(this, &modbusCamera::updateTextEdit, ui->textEdit, &QTextEdit::append);
    connect(this, &modbusCamera::sendRegistorsDataSignal, this, &modbusCamera::onSendRegistorsDataSlot);  // 连接信号与槽
    connect(this, &modbusCamera::displayImage, this, &modbusCamera::updateImageDisplay);
    //OCR 检测结果
    connect(this, &modbusCamera::displayDetectResult, this, &modbusCamera::updateDetectResultDisplay);
    connect(this, &modbusCamera::displayDetectImage, this, &modbusCamera::updateDetectImageDisplay);

    // 初始化寄存器
    connect(timer, &QTimer::timeout, this, &modbusCamera::readRegistersData);

    // 启动定时器，每间隔100ms读取一次寄存器
    timer->start(100);

    // 在构造函数中加载相机列表
    loadCameraList();
}

modbusCamera::~modbusCamera()
{
    stopGrabThread();
#ifndef NO_HUARAY_CAMERA
    if (devHandle != nullptr)
    {
        IMV_StopGrabbing(devHandle);
        IMV_Close(devHandle);
        IMV_DestroyHandle(devHandle);
        devHandle = nullptr;
    }
#endif
    delete ui;
}

// 枚举设备并加载到 ComboBox（始终提供本地测试视频选项）
void modbusCamera::loadCameraList()
{
    ui->comboBox->clear();
    ui->comboBox->addItem(QStringLiteral("本地视频: test1.mp4"), QStringLiteral("local_video"));

#ifdef NO_HUARAY_CAMERA
    ui->textEdit->append(QStringLiteral("已禁用华睿相机 SDK，仅使用本地视频 test1.mp4。"));
#else
    int ret = IMV_EnumDevices(&m_deviceInfoList, interfaceTypeAll);
    if (IMV_OK != ret)
    {
        ui->textEdit->append(QStringLiteral("未检测到华睿相机，可使用本地视频测试。"));
        ui->comboBox->setCurrentIndex(0);
        return;
    }

    for (unsigned int i = 0; i < m_deviceInfoList.nDevNum; ++i)
    {
        ui->comboBox->addItem(m_deviceInfoList.pDevInfo[i].cameraKey, static_cast<int>(i));
    }

    if (m_deviceInfoList.nDevNum == 0)
    {
        ui->textEdit->append(QStringLiteral("相机列表为空，默认使用本地视频 test1.mp4。"));
    }
#endif
    ui->comboBox->setCurrentIndex(0);
}

bool modbusCamera::isLocalVideoSelected() const
{
    return ui->comboBox->currentData().toString() == QLatin1String("local_video");
}

QString modbusCamera::resolveTestVideoPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        // 工程源码目录（开发机固定路径）
        QStringLiteral("D:/tool/Qt/Project/modbusCamera-main/test_picture_video/test1.mp4"),
        // 可执行文件旁（DESTDIR=Bin/x64）
        appDir + QStringLiteral("/test_picture_video/test1.mp4"),
        // 从 shadow build 的 Bin/x64 回到工程根
        appDir + QStringLiteral("/../../../../test_picture_video/test1.mp4"),
        appDir + QStringLiteral("/../../../test_picture_video/test1.mp4"),
        appDir + QStringLiteral("/../../test_picture_video/test1.mp4"),
        QDir::currentPath() + QStringLiteral("/test_picture_video/test1.mp4"),
        QDir::currentPath() + QStringLiteral("/../../test_picture_video/test1.mp4"),
    };

    for (const QString& path : candidates)
    {
        const QFileInfo info(path);
        if (info.exists() && info.isFile())
        {
            qDebug() << "Resolved test video:" << info.absoluteFilePath();
            return info.absoluteFilePath();
        }
    }

    qDebug() << "Test video not found. appDir=" << appDir << "cwd=" << QDir::currentPath();
    return appDir + QStringLiteral("/test_picture_video/test1.mp4");
}

void modbusCamera::stopGrabThread()
{
    g_isExitThread = true;
    if (threadHandle != nullptr)
    {
        //WaitForSingleObject(threadHandle, INFINITE) 阻塞等待线程结束
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
        threadHandle = nullptr;
    }
}
//决定这一帧是"只预览"还是"要做 OCR 检测 + 回写 Modbus"
void modbusCamera::processGrabbedFrame(cv::Mat& image)
{
    //g_captureSignal：本帧是否需要跑 OCR
    if (!g_captureSignal)
    {
        QImage qImage = cvMatToQImage(image);
        emit displayImage(qImage);
        return;
    }

    //触发时跑 OCR
    int ret_code = ocrDet.getDetectResult(image);
    QImage qImage = cvMatToQImage(image);
    emit displayImage(qImage);
    emit displayDetectResult(ret_code);

    send_index = ret_code;
    if (send_index == 1)
    {
        count_ok += 1;
    }

    emit updateTextEdit("count_ok: " + QString::number(count_ok) + " send_index: " + QString::number(send_index));
    //条件判断，决定是否写寄存器
    if (((count_ok == 0) && (send_index == 2)) || ((count_ok == 1) && (send_index == 1)))
    {
        emit displayDetectImage(qImage);
        //请求写 Modbus
        emit sendRegistorsDataSignal();
    }

    g_captureSignal = false;
}

void modbusCamera::onSendRegistorsDataSlot()
{
    sendRegistorsData();  // 在主线程中执行 sendRegistorsData
}


void modbusCamera::readRegistersData()
{
    if (client->state() != QModbusDevice::ConnectedState)
    {
        // 本地视频离线模式不刷屏
        if (!autoTriggerDetect)
            ui->textEdit->append(QString("modbus is not connected"));
        return;
    }

    QModbusDataUnit readDataUnit;
    readDataUnit.setRegisterType(QModbusDataUnit::HoldingRegisters);
    readDataUnit.setStartAddress(OREGS_READ_ADDR_START);
    readDataUnit.setValueCount(OREGS_READ_LEN);

    QModbusReply* reply = client->sendReadRequest(readDataUnit, SLAVE_ID);

    if(reply)
    {
         if(!reply->isFinished())
         {
            connect(reply, &QModbusReply::finished, this, &modbusCamera::replyRegistersData); //异步处理槽函数
            return;
         }
         reply->deleteLater(); // 针对广播消息
    }
}

void modbusCamera::sendRegistorsData()
{
    if (client->state() != QModbusDevice::ConnectedState)
    {
        //bool autoTriggerDetect = false;
        if (!autoTriggerDetect)
            ui->textEdit->append(QString("Modbus is not connected"));
        else
            emit updateTextEdit(QStringLiteral("检测结果(离线): ") + QString::number(send_index));
        return;
    }
    /*
     * 默认构造，m_type	Invalid
     * m_startAddress	-1
     * m_values         空
     * m_valueCount     0
     * isValid()        false
     */
    QModbusDataUnit writeDataUnit;
    //设置寄存器类型
    /*
     *  Invalid,            //Invalid 默认值
        DiscreteInputs,     // 离散输入，只读，1 bit，功能码 0x02
        Coils,              // 线圈，读写，1 bit，功能码 0x01/0x05/0x0F
        InputRegisters,     // 输入寄存器，只读，16 bit，功能码 0x04
        HoldingRegisters    // 保持寄存器，读写，16 bit，功能码 0x03/0x06/0x10
     */
    writeDataUnit.setRegisterType(QModbusDataUnit::HoldingRegisters);
    /*
     * #define OREGS_READ_ADDR_START  100
     * #define OREGS_READ_LEN         2
     * #define OREGS_WRITE_ADDR_START 100
     * #define OREGS_WRITE_LEN        1
     * #define SLAVE_ID               1
     */
    // 设置起始地址
    writeDataUnit.setStartAddress(OREGS_WRITE_ADDR_START);
    // 设置值数量
    writeDataUnit.setValueCount(OREGS_WRITE_LEN);

    QVector<quint16> values;
    //append 在数组末尾追加一个元素
    values.append(send_index);
    writeDataUnit.setValues(values);

    QModbusReply* reply = client->sendWriteRequest(writeDataUnit, SLAVE_ID);

    if (reply)
    {
        // 连接 reply 的 finished 信号到 onModbusReplyFinished 槽
        reply->deleteLater();
    }
    else
    {
        // 如果没有成功创建 reply，更新 UI 显示错误信息
        emit updateTextEdit("Failed to send Modbus write request.");
    }
}

void modbusCamera::replyRegistersData()
{
    QModbusReply* reply = (QModbusReply*)(sender());
    if (reply)
    {
        QModbusDataUnit unit = reply->result();  // 提取reply数据
        reply->deleteLater(); // 释放内存

        if(unit.valueCount() > 0)
        {
            QVector<quint16> data = unit.values();
            // 将外部触发信号修改为true
            if (data[1] == 1)
            {
                g_captureSignal = true;
                ui->textEdit->append("Trigger capture signal received!");
            }
            else
            {
                g_captureSignal = false;
                count_ok = 0;  // 将count_ok置0,为检测做准备
            }

            QString s;
            Q_FOREACH(quint16 i, data)
            {
                s.append(QString::number(i)).append(" "); // 把所有值放到一个字符串中
            }
            ui->textEdit->append(s);
        }
    }
}

#ifndef NO_HUARAY_CAMERA
unsigned __stdcall modbusCamera::frameGrabbingProc(void* pUserData)
{
    int ret = IMV_OK;
    IMV_HANDLE devHandle = (IMV_HANDLE)pUserData;
    IMV_Frame frame;

    if (NULL == devHandle)
    {
        emit instance->updateTextEdit("Invalid device handle.");
        return 0;
    }

    while (!g_isExitThread)
    {
        ret = IMV_GetFrame(devHandle, &frame, 100);
        if (IMV_OK != ret)
        {
            qDebug() << "Get frame failed! ErrorCode[" << ret << "]";
            continue;
        }

        cv::Mat image;
        instance->frameToCVImage(frame, image);
        instance->processGrabbedFrame(image);

        ret = IMV_ReleaseFrame(devHandle, &frame);
        if (IMV_OK != ret)
        {
            emit instance->updateTextEdit("Release frame failed!");
        }

        QThread::msleep(10);
    }
    return 0;
}
#endif
//读帧
unsigned __stdcall modbusCamera::videoGrabbingProc(void* /*pUserData*/)
{
    if (instance == nullptr)
        return 0;

    const std::string videoPath = instance->localVideoPath.toLocal8Bit().constData();
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened())
    {
        emit instance->updateTextEdit(QStringLiteral("无法打开本地视频: ") + instance->localVideoPath);
        return 0;
    }

    emit instance->updateTextEdit(QStringLiteral("本地视频播放中: ") + instance->localVideoPath);

    int frameCounter = 0;
    while (!g_isExitThread)
    {
        cv::Mat image;
        if (!cap.read(image) || image.empty())
        {
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            count_ok = 0;
            continue;
        }

        // 无 Modbus 时周期性自动触发检测，便于离线联调 OCR
        if (instance->autoTriggerDetect)
        {
            ++frameCounter;
            if (frameCounter >= 45)
            {
                frameCounter = 0;
                g_captureSignal = true;
                emit instance->updateTextEdit(QStringLiteral("本地视频自动触发检测"));
            }
        }

        //processGrabbedFrame：单帧处理，决定这一帧是"只预览"还是"要做 OCR 检测 + 回写 Modbus"
        instance->processGrabbedFrame(image);
        QThread::msleep(30);
    }

    cap.release();
    return 0;
}

bool modbusCamera::startLocalVideoMode()
{
    localVideoPath = resolveTestVideoPath();
    if (!QFile::exists(localVideoPath))
    {
        QMessageBox::critical(this, QStringLiteral("视频不存在"),
                              QStringLiteral("找不到测试视频:\n%1").arg(localVideoPath));
        return false;
    }

    useLocalVideo = true;
    //
    stopGrabThread();
    g_isExitThread = false;

    //采集线程
    //videoGrabbingProc读帧
    threadHandle = (HANDLE)_beginthreadex(NULL, 0, videoGrabbingProc, nullptr, 0, NULL);
    if (!threadHandle)
    {
        ui->textEdit->append(QStringLiteral("创建本地视频线程失败"));
        return false;
    }

    ui->textEdit->append(QStringLiteral("本地视频模式已启动。"));
    if (autoTriggerDetect)
        ui->textEdit->append(QStringLiteral("未连接 Modbus，将自动周期性触发 OCR 检测。"));
    else
        ui->textEdit->append(QStringLiteral("已连接 Modbus，由寄存器触发检测。"));

    ui->connect_modbus->setEnabled(false);
    ui->disconnect_modbus->setEnabled(true);
    return true;
}

#ifndef NO_HUARAY_CAMERA
bool modbusCamera::startCameraMode()
{
    useLocalVideo = false;
    autoTriggerDetect = false;

    if (devHandle != nullptr)
    {
        IMV_StopGrabbing(devHandle);
        IMV_Close(devHandle);
        IMV_DestroyHandle(devHandle);
        devHandle = nullptr;
    }

    int ret = IMV_EnumDevices(&m_deviceInfoList, interfaceTypeAll);
    if (IMV_OK != ret)
    {
        ui->textEdit->append(QStringLiteral("Enumeration devices failed!"));
        return false;
    }

    ret = IMV_CreateHandle(&devHandle, modeByIndex, (void*)&cameraIndex);
    if (IMV_OK != ret)
    {
        ui->textEdit->append(QStringLiteral("Create devHandle failed!"));
        QMessageBox::critical(this, "Create devHandle Failed", "Unable to create the devHandle.");
        return false;
    }

    ret = IMV_Open(devHandle);
    if (IMV_OK != ret)
    {
        ui->textEdit->append(QStringLiteral("Failed to open camera."));
        QMessageBox::critical(this, "Camera Open Failed", "Unable to open the camera.");
        return false;
    }

    ret = IMV_StartGrabbing(devHandle);
    if (IMV_OK != ret)
    {
        ui->textEdit->append(QStringLiteral("Failed to start image grabbing."));
        QMessageBox::critical(this, "Image Grabbing Failed", "Unable to start grabbing images.");
        return false;
    }

    stopGrabThread();
    g_isExitThread = false;
    threadHandle = (HANDLE)_beginthreadex(NULL, 0, frameGrabbingProc, (void*)devHandle, 0, NULL);
    if (!threadHandle)
    {
        ui->textEdit->append(QStringLiteral("Failed to create getFrame thread!"));
        return false;
    }

    ui->textEdit->append(QStringLiteral("Image capture started."));
    ui->connect_modbus->setEnabled(false);
    ui->disconnect_modbus->setEnabled(true);
    return true;
}
#endif


void modbusCamera::on_connect_modbus_clicked()
{
    if (ui->comboBox->currentIndex() == -1) {
        QMessageBox::critical(this, "No Source Selected", "Please select a camera or local video.");
        return;
    }

    useLocalVideo = isLocalVideoSelected();
#ifdef NO_HUARAY_CAMERA
    useLocalVideo = true;
#else
    if (useLocalVideo)
        cameraIndex = 0;
    else
        cameraIndex = ui->comboBox->currentData().toInt();
#endif

    QString ip = ui->ip->text();
    QString port = ui->port->text();
    ui->textEdit->append("IP: " + ip + ", Port: " + port);

    client->disconnectDevice();
    client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    client->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    client->setTimeout(500);
    client->setNumberOfRetries(3);

    disconnect(client, &QModbusDevice::stateChanged, this, &modbusCamera::on_modbus_stateChanged);
    disconnect(client, &QModbusClient::errorOccurred, this, &modbusCamera::on_modbus_errorOccurred);
    connect(client, &QModbusDevice::stateChanged, this, &modbusCamera::on_modbus_stateChanged);
    connect(client, &QModbusClient::errorOccurred, this, &modbusCamera::on_modbus_errorOccurred);

    // 本地视频：Modbus 连不上也能离线跑 OCR
    if (useLocalVideo)
    {
        bool connected = client->connectDevice();
        if (!connected || client->state() != QModbusDevice::ConnectedState)
        {
            autoTriggerDetect = true;
            ui->textEdit->append(QStringLiteral("Modbus 未连接，进入本地视频离线模式。"));
            startLocalVideoMode();
            return;
        }
        // 已发起连接，等 on_modbus_stateChanged 启动视频
        autoTriggerDetect = false;
        QTimer* once = new QTimer(this);
        once->setSingleShot(true);
        connect(once, &QTimer::timeout, this, &modbusCamera::on_connectionTimeout);
        once->start(1500);
        return;
    }

#ifndef NO_HUARAY_CAMERA
    QTimer* once = new QTimer(this);
    once->setSingleShot(true);
    connect(once, &QTimer::timeout, this, &modbusCamera::on_connectionTimeout);
    once->start(100);

    if (!client->connectDevice())
    {
        ui->textEdit->append("Connection failed immediately. Please confirm IP and port.");
        QMessageBox::critical(this, "Connection Failed", "Connection failed immediately. Please confirm IP and port are correct.");
        return;
    }
#else
    autoTriggerDetect = true;
    startLocalVideoMode();
#endif
}



void modbusCamera::on_modbus_stateChanged(QModbusDevice::State state)
{
    // 打印当前连接状态
    qDebug() << "Modbus Client state changed:" << state;

    if (state == QModbusDevice::ConnectedState)
    {
        ui->textEdit->append("Connection successful");
        autoTriggerDetect = false;

        if (useLocalVideo || isLocalVideoSelected())
        {
            useLocalVideo = true;
            if (!startLocalVideoMode())
                return;
        }
#ifndef NO_HUARAY_CAMERA
        else
        {
            if (!startCameraMode())
                return;
        }
#else
        else
        {
            useLocalVideo = true;
            autoTriggerDetect = true;
            if (!startLocalVideoMode())
                return;
        }
#endif

        if (timer == nullptr)
            timer = new QTimer(this);

        disconnect(timer, &QTimer::timeout, this, &modbusCamera::readRegistersData);
        connect(timer, &QTimer::timeout, this, &modbusCamera::readRegistersData);
        timer->start(100);
        ui->textEdit->append("Timer started.");
    }
    else if (state == QModbusDevice::UnconnectedState)
    {
        ui->textEdit->append("Connection failed or disconnected.");
        qDebug() << "Modbus Client state: Unconnected";
    }
}

void modbusCamera::on_modbus_errorOccurred(QModbusDevice::Error error)
{
    ui->textEdit->append("Connection failed. Error: " + QString::number(error));
    qDebug() << "Modbus Client error occurred, state:" << client->state();

    // 本地视频模式下允许无 Modbus 继续离线测试
    if (useLocalVideo || isLocalVideoSelected())
    {
        autoTriggerDetect = true;
        useLocalVideo = true;
        if (threadHandle == nullptr)
            startLocalVideoMode();
        return;
    }

    QMessageBox::critical(this, "Connection Failed", "Connection failed with error: " + QString::number(error));
}

void modbusCamera::on_disconnect_modbus_clicked()
{
    stopGrabThread();
    autoTriggerDetect = false;

#ifndef NO_HUARAY_CAMERA
    if (devHandle != nullptr)
    {
        IMV_StopGrabbing(devHandle);
        IMV_Close(devHandle);
        IMV_DestroyHandle(devHandle);
        devHandle = nullptr;
    }
#endif

    client->disconnectDevice();
    ui->textEdit->clear();
    ui->image_label->clear();
    ui->light_label->clear();
    ui->det_image_label->clear();

    if (timer != nullptr)
        timer->stop();
    disconnect(timer, &QTimer::timeout, this, &modbusCamera::readRegistersData);

    client->disconnectDevice();
    ui->connect_modbus->setEnabled(true);
    ui->disconnect_modbus->setEnabled(true);
    count_ok = 0;
    useLocalVideo = false;
}

void modbusCamera::on_comboBox_activated(int index)
{
    ui->comboBox->setCurrentIndex(index);
    if (isLocalVideoSelected())
    {
        useLocalVideo = true;
#ifndef NO_HUARAY_CAMERA
        cameraIndex = 0;
#endif
        ui->textEdit->append(QStringLiteral("已选择本地测试视频 test1.mp4"));
    }
    else
    {
#ifdef NO_HUARAY_CAMERA
        useLocalVideo = true;
#else
        useLocalVideo = false;
        cameraIndex = ui->comboBox->currentData().toInt();
#endif
    }
}

void modbusCamera::on_connectionTimeout()
{
    if (client->state() == QModbusDevice::ConnectedState)
        return;

    if (useLocalVideo || isLocalVideoSelected())
    {
        autoTriggerDetect = true;
        useLocalVideo = true;
        if (threadHandle == nullptr)
            startLocalVideoMode();
        ui->textEdit->append(QStringLiteral("Modbus 超时，已切换本地视频离线模式。"));
        return;
    }

    QMessageBox::critical(this, "Connection Timeout", "Failed to connect to the Modbus device. Please check the IP and port.");
}

void modbusCamera::updateImageDisplay(const QImage& image)
{
    // 在主线程中更新 QLabel 上的图像
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        QMetaObject::invokeMethod(this, [this, image]{
            // ui更新操作
            ui->image_label->setPixmap(QPixmap::fromImage(image).scaled(ui->image_label->size(), Qt::KeepAspectRatio));
        });
        return;
    }

    // 如果已经在主线程，直接更新
    ui->image_label->setPixmap(QPixmap::fromImage(image).scaled(ui->image_label->size(), Qt::KeepAspectRatio));
    ui->image_label->update();
}

void modbusCamera::updateDetectResultDisplay(int ret_code)
{
    QImage image;
    QString lightPath = NULL;

    // 表示没检测到字符
    if (ret_code == 0)
    {
        lightPath = ":/res/state_gray.jpg";
    }
    // 表示字符符合规则(OK)
    else if (ret_code == 1)
    {
        lightPath = ":/res/state_green.png";
    }
    // 表示字符不符合规则(NG)
    else
    {
        lightPath = ":/res/state_red.png";
    }
    // 载入图像
    image.load(lightPath);

    // 在主线程中更新 QLabel 上的图像
    //QThread::currentThread() : 返回当前正在执行这段代码的线程的 QThread* 指针
    //QCoreApplication::instance()->thread() : 返回全局唯一的 QCoreApplication 对象指针, QObject::thread() 返回该对象所属的线程。
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        QMetaObject::invokeMethod(this, [this, image]{
            // ui更新操作
            ui->light_label->setPixmap(QPixmap::fromImage(image).scaled(ui->light_label->size(), Qt::KeepAspectRatio));
        });
        return;
    }

    // 如果已经在主线程，直接更新
    ui->light_label->setPixmap(QPixmap::fromImage(image).scaled(ui->light_label->size(), Qt::KeepAspectRatio));
    ui->light_label->update();
}

void modbusCamera::updateDetectImageDisplay(const QImage& detectImage)
{
    // 在主线程中更新显示检测图像的 QLabel
    if (QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        QMetaObject::invokeMethod(this, [this, detectImage]{
            // 更新 ui 上的检测图像标签
            ui->det_image_label->setPixmap(QPixmap::fromImage(detectImage).scaled(ui->det_image_label->size(), Qt::KeepAspectRatio));
        });
        return;
    }

    // 如果已经在主线程，直接更新
    ui->det_image_label->setPixmap(QPixmap::fromImage(detectImage).scaled(ui->det_image_label->size(), Qt::KeepAspectRatio));
    ui->det_image_label->update();
}


#ifndef NO_HUARAY_CAMERA
void modbusCamera::frameToCVImage(const IMV_Frame& pFrame, cv::Mat& image)
{
    unsigned int width = pFrame.frameInfo.width;
    unsigned int height = pFrame.frameInfo.height;
    IMV_EPixelType pixelFormat = pFrame.frameInfo.pixelFormat;

    if (pFrame.pData == NULL)
    {
        printf("Frame data is NULL\n");
        return;
    }

    cv::Mat gray_image;
    switch (pixelFormat)
    {
    case gvspPixelMono8:
        gray_image = cv::Mat(height, width, CV_8UC1, pFrame.pData);
        cv::cvtColor(gray_image, image, cv::COLOR_GRAY2BGR);
        break;
    case gvspPixelBGR8:
        image = cv::Mat(height, width, CV_8UC3, pFrame.pData);
        break;
    default:
        printf("Unsupported pixel format: %d\n", pixelFormat);
        return;
    }
}
#endif

QImage modbusCamera::cvMatToQImage(const cv::Mat& inMat)
{
    switch (inMat.type())
    {
        // 8-bit, 4 channel
        case CV_8UC4: {
            QImage image(
                inMat.data,//图像数据
                inMat.cols,//图像宽度
                inMat.rows,//图像高度
                static_cast<int>(inMat.step),//图像矩阵元素类型
                QImage::Format_ARGB32//图像的像素格式
            );
            return image;
        }

        // 8-bit, 3 channel
        case CV_8UC3: {
            QImage image(inMat.data,
                inMat.cols,
                inMat.rows,
                static_cast<int>(inMat.step),
                QImage::Format_RGB888);
            return image.rgbSwapped(); //彩色图像为RGB三通道，交换R和B通道
        }

        // 8-bit, 1 channel
        case CV_8UC1: {
            static QVector<QRgb> sColorTable;
            // 我们只需要初始化一次颜色表
            if (sColorTable.isEmpty()) {
                for (int i = 0; i < 256; ++i) {
                    sColorTable.push_back(qRgb(i, i, i));
                }
            }
            QImage image(inMat.data,
                inMat.cols,
                inMat.rows,
                static_cast<int>(inMat.step),
                QImage::Format_Indexed8);
            image.setColorTable(sColorTable);
            return image;
        }
        default:
            break;
    }
    return QImage();
}
