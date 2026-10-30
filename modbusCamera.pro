QT       += core gui serialbus network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += NOMINMAX

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# 无华睿相机时默认关闭 SDK（本地视频测试）。有相机时改为: CONFIG -= no_huaray_camera
CONFIG += no_huaray_camera

DESTDIR = Bin/x64
INCLUDEPATH += Include

# 编译后把测试视频复制到输出目录
TEST_VIDEO_SRC = $$shell_path($$PWD/test_picture_video/test1.mp4)
TEST_VIDEO_DST_DIR = $$shell_path($$OUT_PWD/$$DESTDIR/test_picture_video)
QMAKE_POST_LINK += $$quote(cmd /c if not exist \"$$TEST_VIDEO_DST_DIR\" mkdir \"$$TEST_VIDEO_DST_DIR\") $$escape_expand(\\n\\t)
QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$TEST_VIDEO_SRC\" \"$$TEST_VIDEO_DST_DIR\\test1.mp4\")

no_huaray_camera {
    DEFINES += NO_HUARAY_CAMERA
    message("Build without HuaRay camera SDK (local video mode)")
} else {
    LIBS += -L$$PWD/Depends/x64/vs2013shared -lMVSDKmd
    LIBS += -L$$PWD/Depends/x64/vs2013shared -lVideoRender
}


# 配置OPENCV环境
INCLUDEPATH += D:/tool/OpenCV/opencv/build/include \
               D:/tool/OpenCV/opencv/build/include/opencv \
               D:/tool/OpenCV/opencv/build/include/opencv2

# 添加v15版 opencv 库文件，区分debug和release
LIBS += -LD:/tool/OpenCV/opencv/build/x64/vc15/lib/ -lopencv_world3410 \
        -LD:/tool/OpenCV/opencv/build/x64/vc15/lib/ -lopencv_world3410d

INCLUDEPATH += D:/tool/OpenCV/opencv/build/x64/vc15
DEPENDPATH += D:/tool/OpenCV/opencv/build/x64/vc15

# NCNN环境
NCNN_INSTALL = D:/tool/Qt/Project/ncnn-20240820-full-source/build/install
INCLUDEPATH += $$NCNN_INSTALL/include/ncnn
DEPENDPATH  += $$NCNN_INSTALL/include/ncnn
LIBS += -L$$NCNN_INSTALL/lib
CONFIG(debug, debug|release) {
    LIBS += -lncnnd
} else {
    LIBS += -lncnn
}

SOURCES += \
    camera_hub.cpp \
    camera_tile_view.cpp \
    clipper.cpp \
    crnnNet.cpp \
    dbNet.cpp \
    detail_navigator.cpp \
    detail_view.cpp \
    hmi_presenter.cpp \
    main.cpp \
    main_interface.cpp \
    modbuscamera.cpp \
    ocrDetect.cpp \
    ocr_scheduler.cpp \
    ocr_worker.cpp \
    settingsform.cpp

HEADERS += \
    baseOcr.h \
    baseStruct.h \
    boxSorter.h \
    camera_hub.h \
    camera_session.h \
    camera_tile_view.h \
    clipper.hpp \
    crnnNet.h \
    dbNet.h \
    detail_navigator.h \
    detail_view.h \
    hmi_presenter.h \
    image_convert.h \
    main_interface.h \
    modbuscamera.h \
    ocrDetect.h \
    ocr_scheduler.h \
    ocr_worker.h \
    settingsform.h \
    tools.h

FORMS += \
    camera_tile.ui \
    detail_view.ui \
    main_interface.ui \
    modbuscamera.ui \
    settingsform.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc


