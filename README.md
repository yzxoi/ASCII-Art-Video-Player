# ASCII Art Video Player

This project, an ASCII art video player that converts images and videos into ASCII character art and displays them in the console, is one of labworks of Advanced Language Program Design, Tongji Univ. The program supports single image display, image sequence playback, and video file playback with automatic frame extraction and adaptive frame rate calculation. It uses Windows API (FastPrinter.h) for fast console rendering and FFmpeg for video processing.

## Quick Start

由于需要调用 Windows API，本项目需要在 Windows 平台 Visual Studio 2022（或更高版本）运行。

同时，为了实现自适应帧率控制，本项目内部调用了 FFmpeg 用于视频处理，请确保：**FFmpeg 程序在项目根目录或在系统 PATH 中**。

### Compile & Run
- 使用 Visual Studio 2022 打开项目文件
- 选择 Release 配置，**开启 O2 优化**。
- 编译项目

Run & Have Fun!

## 文件结构

```
Z_4_Resources/
├── main.cpp              # 主程序文件
├── Array.h               # Array 类实现
├── PicReader.h           # 图片读取类
├── FastPrinter.h         # 快速绘制类
├── ffmpeg.exe            # FFmpeg 可执行文件
├── classic_picture/      # 测试图片目录
│   ├── lena.jpg
│   ├── airplane.jpg
│   └── ...
├── test/                 # 测试图片目录
│   ├── black.png
│   ├── white.png
│   └── ...
└── temp_frames/          # 临时帧目录（自动创建）
    ├── frame_0001.png
    ├── frame_0002.png
    └── audio.wav
```

## 日志文件

程序运行时会自动创建日志文件：
- 文件名格式：`ascii_player_log_YYYYMMDD_HHMMSS.txt`
- 保存位置：程序运行目录
- 包含内容：所有操作记录、错误信息、性能数据

## Q&A

如果播放卡顿，你可以：
- 减小控制台窗口尺寸
- 使用灰度模式（修改代码）
- 降低目标帧率
- 减小视频分辨率（FFmpeg 会自动缩放）
- **升级你的电脑**