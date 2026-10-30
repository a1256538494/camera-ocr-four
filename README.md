# 文档目录（docs）

本目录存放 **多路总控 HMI** 相关设计与实现说明。  
仓库根目录 [README.md](../README.md) 描述的是早期 **单路 Demo**（`modbusCamera` + 华睿/Modbus）；当前默认入口已切到多路总览。

---

## 当前产品形态（摘要）

| 项 | 说明 |
|----|------|
| 入口 | `main.cpp` → `main_interface`（多路总览） |
| 相机数 | 4 路，宫格固定 **2×2** |
| 视频 | `test_picture_video/test1.mp4` … `test4.mp4`（Cam01–04 一一对应） |
| OCR | 独立线程；本地阶段 **100ms 模拟触发**（暂可不连 Modbus Slave） |
| 详情 | 点击 Tile → `DetailView`（只订阅画面，不二次开流） |
| 设置 | `settingsform` 应用模态对话框（主界面不隐藏、不可点） |

---

## 架构速览

```text
main_interface（壳：顶栏 / 空宫格容器 / 侧栏 / 底栏）
  ├── CameraTileView ×4     ← camera_tile.ui，运行时装入 2×2
  ├── CameraHub             ← 四路 VideoCapture
  ├── OcrScheduler          ← 100ms 脉冲 → OcrWorker（QThread）
  ├── HmiPresenter          ← 刷状态、汇总、事件列表
  ├── DetailNavigator       ← hide 总览 ↔ DetailView
  └── settingsform          ← 左导航 + QStackedWidget 多页
```

**约定：** 四路 Tile 与详情共用 `CameraSession`，详情禁止再 `open` 一路流。

---

## 主要源码与界面

| 模块 | 文件 |
|------|------|
| 主壳 | `main_interface.h/.cpp/.ui` |
| 宫格单元 | `camera_tile_view.*` + `camera_tile.ui` |
| 采集 | `camera_hub.*`、`camera_session.h` |
| OCR 调度/工人 | `ocr_scheduler.*`、`ocr_worker.*`、`ocrDetect.*` |
| 详情 | `detail_view.*` + `detail_view.ui`、`detail_navigator.*` |
| 设置 | `settingsform.*` + `settingsform.ui` |
| UI 编排 | `hmi_presenter.*` |
| 旧单路 Demo | `modbuscamera.*`（仍保留，默认不作为入口） |

---

## 本地联调步骤

1. 确认 `test_picture_video/` 下存在 `test1.mp4`–`test4.mp4`。  
2. Qt Creator 打开 `modbusCamera.pro`，选用本机 Qt/OpenCV/NCNN 配置，**qmake + 编译**。  
3. 运行后点 **全部开始**：四路应分别播对应测试视频；OCR 按模拟节拍出 OK/NG。  
4. 点某一格进入详情；**设置** 打开模态配置窗（IP/日志路径等，持久化可后续接 ConfigService）。  
5. **全部停止** 应停采集与新 OCR 投递（软停会 abort/断连，避免队列继续刷结果）。

若从 `Bin/x64` 启动，请把 `test2`–`test4` 一并拷到运行目录的 `test_picture_video/`，或依赖工程绝对路径候选。

---

## 相关路径

- 测试视频：`../test_picture_video/`  
- 工程文件：`../modbusCamera.pro`  
- 权重等：`../weights/`（OCR 模型，以 `ocrDetect` 内路径为准）
