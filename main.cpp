#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <io.h>
#include <direct.h>
#include <mmsystem.h>
#include <ctime>

#pragma comment(lib, "winmm.lib")

#include "Array.h"
#include "PicReader.h"
#include "FastPrinter.h"

using namespace std;

string g_ffmpegPath = "";

ofstream g_logFile;

string g_logFilePath = "";

/**
 * 日志输出函数：同时输出到控制台和日志文件
 */
void log(const string& message) {
    cout << message;
    if (g_logFile.is_open()) {
        g_logFile << message;
        g_logFile.flush();  // 立即刷新，确保日志及时写入
    }
}

/**
 * 日志输出函数（带换行）
 */
void logln(const string& message = "") {
    log(message + "\n");
}

/**
 * 初始化日志文件
 */
bool initializeLogFile() {
    time_t now = time(0);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    
    char logFileName[256];
    sprintf(logFileName, "ascii_player_log_%04d%02d%02d_%02d%02d%02d.txt",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    g_logFilePath = logFileName;
    
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    string fullPath = string(currentDir) + "\\" + logFileName;
    
    g_logFile.open(logFileName, ios::out | ios::trunc);
    if (!g_logFile.is_open()) {
        g_logFile.open(fullPath, ios::out | ios::trunc);
        if (!g_logFile.is_open()) {
            return false;
        }
        g_logFilePath = fullPath;
    }
    
    char header[256];
    sprintf(header, "=== ASCII Player Log - %04d-%02d-%02d %02d:%02d:%02d ===\n",
            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    g_logFile << header;
    g_logFile.flush();
    
    return true;
}

/**
 * 关闭日志文件
 */
void closeLogFile() {
    if (g_logFile.is_open()) {
        logln("=== Log file closed ===");
        g_logFile.close();
    }
}

// 将灰度分为 15 级，每一级由一个 ascii 字符表示，强度越大 ascii 字符内容越少 (越白)
static const char ASCII_TABLE[] = {
    'M','N','H','Q','$','O','C','?','7','>','!',':','-',';','.'
};
static const int ASCII_LEVELS = sizeof(ASCII_TABLE) / sizeof(ASCII_TABLE[0]);

inline unsigned char rgb_to_gray(unsigned char r,
    unsigned char g,
    unsigned char b)
{
    int gray = (299 * (int)r + 587 * (int)g + 114 * (int)b) / 1000;
    if (gray < 0) gray = 0;
    if (gray > 255) gray = 255;
    return (unsigned char)gray;
}

inline char gray_to_ascii(unsigned char gray)
{
    unsigned char asciiIndex = gray / 18;
    if (asciiIndex >= ASCII_LEVELS) asciiIndex = ASCII_LEVELS - 1;
    return ASCII_TABLE[asciiIndex];
}

/**
 * 使用 PicReader 读取图片 -> 填充 RGBA Array(H,W,4) & 灰度 Array(H,W)
 * 使用 getData() 方法获取 Array 类型的数据
 */
bool loadImageToArray(PicReader& reader,
    const char* path,
    Array& rgbaArray,
    Array& grayArray,
    UINT& width,
    UINT& height)
{
    reader.readPic(path);
    rgbaArray = reader.getData();
    width = reader.getWidth();
    height = reader.getHeight();
    grayArray = Array((int)height, (int)width);

    for (UINT h = 0; h < height; ++h) {
        for (UINT w = 0; w < width; ++w) {
            int r = rgbaArray[(int)h][(int)w][0];
            int g = rgbaArray[(int)h][(int)w][1];
            int b = rgbaArray[(int)h][(int)w][2];
            unsigned char gval = rgb_to_gray((unsigned char)r,
                (unsigned char)g,
                (unsigned char)b);
            grayArray[(int)h][(int)w] = (int)gval;
        }
    }

    return true;
}

/**
 * 将 RGBA Array 映射成一帧彩色 ASCII 字符画
 * 使用原图的 RGB 颜色来设置字符颜色
 */
void rgbaToAsciiBuffers(const Array& rgba,
    UINT imgW,
    UINT imgH,
    char* dataBuffer,
    BYTE* frontColorBuffer,
    BYTE* backColorBuffer,
    int consoleW,
    int consoleH)
{
    // 控制台字符高度是宽度的2倍
    double scaleX = (double)imgW / (consoleW * 2.0);   // 水平方向：2个字符 = 1个像素的视觉宽度
    double scaleY = (double)imgH / consoleH;           // 垂直方向：1个字符 = 1个像素的视觉高度

    for (int row = 0; row < consoleH; ++row) {
        // [0, consoleH-1] -> [0, imgH-1]
        double yPos = (row + 0.5) * scaleY;
        int yy = (int)yPos;
        if (yy >= (int)imgH) yy = (int)imgH - 1;
        if (yy < 0) yy = 0;
        
        for (int col = 0; col < consoleW; ++col) {
            double xPos = (col + 0.5) * scaleX * 2.0;
            int xx = (int)xPos;
            if (xx >= (int)imgW) xx = (int)imgW - 1;
            if (xx < 0) xx = 0;

            int r = rgba[yy][xx][0];
            int g = rgba[yy][xx][1];
            int b = rgba[yy][xx][2];
            
            unsigned char grayVal = rgb_to_gray((unsigned char)r, (unsigned char)g, (unsigned char)b);
            char ch = gray_to_ascii(grayVal);

            int idx = row * consoleW + col;
            dataBuffer[idx] = ch;
            
            frontColorBuffer[idx * 3 + 0] = (BYTE)r;
            frontColorBuffer[idx * 3 + 1] = (BYTE)g;
            frontColorBuffer[idx * 3 + 2] = (BYTE)b;
            
            backColorBuffer[idx * 3 + 0] = 0;
            backColorBuffer[idx * 3 + 1] = 0;
            backColorBuffer[idx * 3 + 2] = 0;
        }
    }
}

void showAsciiImage(const char* path)
{
    PicReader reader;
    Array rgbaArr, grayArr;
    UINT w = 0, h = 0;

    if (!loadImageToArray(reader, path, rgbaArr, grayArr, w, h)) {
        cout << "Load image failed: " << path << endl;
        return;
    }

    // 根据图片宽高比和字符宽高比（2:1）计算合适的控制台尺寸
    // 控制台字符高度是宽度的2倍，所以要保持宽高比，水平方向需要除以2
    const int MAX_CONSOLE_W = 120;  // 最大控制台宽度
    const int MAX_CONSOLE_H = 60;   // 最大控制台高度
    
    // Image: w/h，Console: (consoleW/2)/consoleH
    // w/h = (consoleW/2)/consoleH => consoleW = 2*w*consoleH/h
    int consoleH, consoleW;
    
    consoleH = MAX_CONSOLE_H;
    consoleW = (int)(2.0 * w * consoleH / h + 0.5);
    
    if (consoleW > MAX_CONSOLE_W) {
        consoleW = MAX_CONSOLE_W;
        consoleH = (int)(h * consoleW / (2.0 * w) + 0.5);
    }
    if (consoleH > MAX_CONSOLE_H) {
        consoleH = MAX_CONSOLE_H;
        consoleW = (int)(2.0 * w * consoleH / h + 0.5);
    }
    
    if (consoleW < 40) consoleW = 40;
    if (consoleH < 20) consoleH = 20;
    if (consoleW > MAX_CONSOLE_W) consoleW = MAX_CONSOLE_W;
    if (consoleH > MAX_CONSOLE_H) consoleH = MAX_CONSOLE_H;


    FastPrinter printer(consoleW, consoleH);

    char* dataBuffer = new char[consoleW * consoleH];
    BYTE* frontColor = new BYTE[consoleW * consoleH * 3];
    BYTE* backColor = new BYTE[consoleW * consoleH * 3];

    rgbaToAsciiBuffers(rgbaArr, w, h, dataBuffer, frontColor, backColor, consoleW, consoleH);

    printer.setData(dataBuffer, frontColor, backColor);
    printer.draw(true);

    cout << "\nPress Enter to exit...";
    getchar();

    delete[] dataBuffer;
    delete[] frontColor;
    delete[] backColor;
}

/**
 * 初始化并查找 ffmpeg.exe 的完整路径
 * --> if no, return false
 */
bool initializeFFmpeg() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != string::npos) {
        exeDir = exeDir.substr(0, lastSlash + 1);
    }
    
    string possiblePaths[] = {
        exeDir + "ffmpeg.exe",
        exeDir + "..\\ffmpeg.exe",
        exeDir + "..\\..\\ffmpeg.exe",
        exeDir + "..\\..\\..\\ffmpeg.exe",
        "ffmpeg.exe"
    };
    
    logln("Searching for ffmpeg.exe...");
    
    for (const string& path : possiblePaths) {
        DWORD fileAttr = GetFileAttributesA(path.c_str());
        if (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
            g_ffmpegPath = path;
            logln("Found ffmpeg.exe at: " + g_ffmpegPath);
            return true;
        }
    }
    
    logln("ffmpeg.exe not found in project directory. Trying system PATH...");
    
    char testCmd[256] = "\"ffmpeg.exe\" -version >nul 2>&1";
    int result = system(testCmd);
    if (result == 0) {
        g_ffmpegPath = "ffmpeg.exe";
        logln("Found ffmpeg.exe in system PATH.");
        return true;
    }
    
    logln("ERROR: ffmpeg.exe not found!");
    logln("Please ensure ffmpeg.exe is:");
    logln("  1. In the same directory as this program");
    logln("  2. In the project root directory");
    logln("  3. In the system PATH");
    logln("Searched locations:");
    for (const string& path : possiblePaths) {
        logln("  - " + path);
    }
    
    return false;
}

/**
 * 去除首尾引号和空格
 */
string cleanPath(const char* path) {
    string cleaned = path;
    while (!cleaned.empty() && cleaned[0] == ' ') {
        cleaned = cleaned.substr(1);
    }
    while (!cleaned.empty() && cleaned.back() == ' ') {
        cleaned.pop_back();
    }
    if (!cleaned.empty() && cleaned[0] == '"') {
        cleaned = cleaned.substr(1);
    }
    if (!cleaned.empty() && cleaned.back() == '"') {
        cleaned.pop_back();
    }
    return cleaned;
}

/**
 * 调用 ffmpeg 提取视频帧到 PNG
 */
bool extractVideoFrames(const char* videoPath, const char* outputPattern, int maxFrames = -1) {
    if (g_ffmpegPath.empty()) {
        logln("ERROR: ffmpeg.exe path not initialized!");
        return false;
    }
    
    string cleanVideoPath = cleanPath(videoPath);
    string cleanOutputPattern = cleanPath(outputPattern);
    
    string ffmpegCmd = g_ffmpegPath;
    if (g_ffmpegPath.find(' ') != string::npos || g_ffmpegPath.find('\\') != string::npos || g_ffmpegPath.find('/') != string::npos) {
        if (g_ffmpegPath != "ffmpeg.exe") {
            ffmpegCmd = "\"" + g_ffmpegPath + "\"";
        }
    }
    
    char cmd[2048];
    if (maxFrames > 0) {
        sprintf(cmd, "%s -i \"%s\" -vf \"scale=320:240\" -frames:v %d \"%s\" -y", 
                ffmpegCmd.c_str(), cleanVideoPath.c_str(), maxFrames, cleanOutputPattern.c_str());
    } else {
        sprintf(cmd, "%s -i \"%s\" -vf \"scale=320:240\" \"%s\" -y", 
                ffmpegCmd.c_str(), cleanVideoPath.c_str(), cleanOutputPattern.c_str());
    }
    
    logln("Extracting video frames... (this may take a while)");
    logln("Using ffmpeg: " + g_ffmpegPath);
    logln("Command: " + string(cmd));
    int result = system(cmd);
    return result == 0;
}

/**
 * 调用 ffmpeg 提取音频
 */
bool extractAudio(const char* videoPath, const char* audioPath) {
    if (g_ffmpegPath.empty()) {
        logln("ERROR: ffmpeg.exe path not initialized!");
        return false;
    }
    
    string cleanVideoPath = cleanPath(videoPath);
    string cleanAudioPath = cleanPath(audioPath);
    
    string ffmpegCmd = g_ffmpegPath;
    if (g_ffmpegPath.find(' ') != string::npos || g_ffmpegPath.find('\\') != string::npos || g_ffmpegPath.find('/') != string::npos) {
        if (g_ffmpegPath != "ffmpeg.exe") {
            ffmpegCmd = "\"" + g_ffmpegPath + "\"";
        }
    }
    
    char cmd[2048];
    sprintf(cmd, "%s -i \"%s\" -vn -acodec pcm_s16le -ar 44100 -ac 2 \"%s\" -y", 
            ffmpegCmd.c_str(), cleanVideoPath.c_str(), cleanAudioPath.c_str());
    
    logln("Extracting audio... (this may take a while)");
    logln("Using ffmpeg: " + g_ffmpegPath);
    logln("Command: " + string(cmd));
    int result = system(cmd);
    return result == 0;
}

/**
 * 自适应帧率计算：测试几帧，计算最佳帧率
 */
int calculateOptimalFPS(const char* pattern, int startIndex, int testFrames = 5) {
    const int CONSOLE_W = 120;
    const int CONSOLE_H = 40;
    
    FastPrinter printer(CONSOLE_W, CONSOLE_H);
    char* dataBuffer = new char[CONSOLE_W * CONSOLE_H];
    BYTE* frontColor = new BYTE[CONSOLE_W * CONSOLE_H * 3];
    BYTE* backColor = new BYTE[CONSOLE_W * CONSOLE_H * 3];
    
    PicReader reader;
    Array rgbaArr, grayArr;
    UINT w = 0, h = 0;
    
    vector<DWORD> frameTimes;
    
    char logMsg[256];
    sprintf(logMsg, "Testing %d frames to calculate optimal FPS...", testFrames);
    logln(logMsg);
    
    for (int i = 0; i < testFrames; ++i) {
        int idx = startIndex + i;
        char path[512];
        sprintf(path, pattern, idx);
        
        HANDLE testFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (testFile == INVALID_HANDLE_VALUE) {
            logln("Test frame not found: " + string(path));
            break;
        }
        CloseHandle(testFile);
        
        DWORD frameStart = GetTickCount();
        
        if (!loadImageToArray(reader, path, rgbaArr, grayArr, w, h)) {
            logln("Failed to load test frame: " + string(path));
            break;
        }
        
        rgbaToAsciiBuffers(rgbaArr, w, h, dataBuffer, frontColor, backColor, CONSOLE_W, CONSOLE_H);
        printer.setData(dataBuffer, frontColor, backColor);
        printer.draw(true);
        
        DWORD frameEnd = GetTickCount();
        DWORD frameTime = frameEnd - frameStart;
        frameTimes.push_back(frameTime);
        
        char logMsg[256];
        sprintf(logMsg, "Test frame %d: %dms", i + 1, frameTime);
        logln(logMsg);
    }
    
    delete[] dataBuffer;
    delete[] frontColor;
    delete[] backColor;
    
    if (frameTimes.empty()) {
        return 10; // default
    }
    
    DWORD totalTime = 0;
    for (DWORD t : frameTimes) {
        totalTime += t;
    }
    DWORD avgTime = totalTime / frameTimes.size();
    
    int optimalFPS = (int)(1000.0 / (avgTime * 1.2));
    
    if (optimalFPS < 5) optimalFPS = 5;
    if (optimalFPS > 30) optimalFPS = 30;
    
    //char logMsg[256];
    sprintf(logMsg, "Average frame processing time: %dms", avgTime);
    logln(logMsg);
    sprintf(logMsg, "Calculated optimal FPS: %d", optimalFPS);
    logln(logMsg);
    
    return optimalFPS;
}

/**
 * 播放 ASCII 视频并同步音频
 */
void playAsciiVideoWithAudio(const char* pattern, int startIndex, int endIndex, 
                             int fps, const char* audioPath) {
    if (fps <= 0) fps = 25;
    
    const int CONSOLE_W = 120;
    const int CONSOLE_H = 40;
    
    FastPrinter printer(CONSOLE_W, CONSOLE_H);
    char* dataBuffer = new char[CONSOLE_W * CONSOLE_H];
    BYTE* frontColor = new BYTE[CONSOLE_W * CONSOLE_H * 3];
    BYTE* backColor = new BYTE[CONSOLE_W * CONSOLE_H * 3];
    
    PicReader reader;
    Array rgbaArr, grayArr;
    UINT w = 0, h = 0;
    
    if (audioPath && strlen(audioPath) > 0) {
        char wavPath[512];
        strcpy(wavPath, audioPath);
        char* ext = strrchr(wavPath, '.');
        if (ext) {
            strcpy(ext, ".wav");
        } else {
            strcat(wavPath, ".wav");
        }
        
        // audio to wav
        if (strcmp(audioPath, wavPath) != 0) {
            char convertCmd[1024];
            sprintf(convertCmd, "ffmpeg -i \"%s\" \"%s\" -y", audioPath, wavPath);
            system(convertCmd);
        }
        
        PlaySoundA(wavPath, NULL, SND_FILENAME | SND_ASYNC);
        logln("Audio playback started.");
    }
    
    DWORD startTick = GetTickCount();
    double frameDuration = 1000.0 / fps;
    
    for (int idx = startIndex; idx <= endIndex; ++idx) {
        DWORD frameStartTick = GetTickCount();
        
        double targetMs = (idx - startIndex + 1) * frameDuration;
        DWORD targetTick = startTick + (DWORD)(targetMs + 0.5);
        DWORD now = GetTickCount();
        
        if (now > targetTick) {
            cout << "Missed Frame " << idx << endl;
            logln("[Player] Missed Frame " + to_string(idx));
            continue;
        }
        
        char path[512];
        sprintf(path, pattern, idx);
        
        HANDLE testFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (testFile == INVALID_HANDLE_VALUE) {
            cout << "Frame file not found: " << path << " (FAILED)" << endl;
            logln("[Player] Frame file not found: " + (string)path + "(FAILED)");
            break;
        }
        CloseHandle(testFile);
        
        if (!loadImageToArray(reader, path, rgbaArr, grayArr, w, h)) {
            cout << "Load frame failed: " << path << " (skipping)" << endl;
            logln("[Player] Load frame failed: " + (string)path + " (skipping)");
            continue;
        }
        
        rgbaToAsciiBuffers(rgbaArr, w, h, dataBuffer, frontColor, backColor, CONSOLE_W, CONSOLE_H);
        printer.setData(dataBuffer, frontColor, backColor);
        printer.draw(true);
        
        DWORD frameEndTick = GetTickCount();
        DWORD frameProcessTime = frameEndTick - frameStartTick;
        
        targetMs = (idx - startIndex + 1) * frameDuration;
        targetTick = startTick + (DWORD)(targetMs + 0.5);
        now = GetTickCount();
        
        if (idx % 10 == 0) {
            cout << "Frame " << idx << " processed in " << frameProcessTime << "ms" << endl;
            logln("[Player] Frame " + to_string(idx) + " processed in " + to_string(frameProcessTime) + "ms");
        }
        
        if (now < targetTick) {
            Sleep(targetTick - now);
        }
    }
    
    // 停止音频
    PlaySoundA(NULL, NULL, 0);
    
    delete[] dataBuffer;
    delete[] frontColor;
    delete[] backColor;
}

/**
 * 多帧图片序列 → ASCII 视频播放
 *
 * pattern example: "video\\frame_%04d.png"
 * startIndex, endIndex
 * fps
 */
void playAsciiVideo(const char* pattern,
    int startIndex,
    int endIndex,
    int fps)
{
    if (fps <= 0) fps = 25;

    const int CONSOLE_W = 120;
    const int CONSOLE_H = 40;

    FastPrinter printer(CONSOLE_W, CONSOLE_H);

    char* dataBuffer = new char[CONSOLE_W * CONSOLE_H];
    BYTE* frontColor = new BYTE[CONSOLE_W * CONSOLE_H * 3];
    BYTE* backColor = new BYTE[CONSOLE_W * CONSOLE_H * 3];

    PicReader reader;
    Array rgbaArr, grayArr;
    UINT w = 0, h = 0;

    DWORD startTick = GetTickCount();
    double frameDuration = 1000.0 / fps;

    for (int idx = startIndex; idx <= endIndex; ++idx) {
        DWORD frameStartTick = GetTickCount();
        
        double targetMs = (idx - startIndex + 1) * frameDuration;
        DWORD targetTick = startTick + (DWORD)(targetMs + 0.5);
        DWORD now = GetTickCount();
        if (now > targetTick) {
            cout << "Missed Frame " << idx << endl;
            logln("[Player] Missed Frame " + to_string(idx));
            continue;
        }
        char path[512];
        sprintf(path, pattern, idx);

        HANDLE testFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (testFile == INVALID_HANDLE_VALUE) {
            cout << "Frame file not found: " << path << " (FAILED)" << endl;
            logln("[Player] Frame file not found: " + (string)path + "(FAILED)");
            break;
        }
        CloseHandle(testFile);

        if (!loadImageToArray(reader, path, rgbaArr, grayArr, w, h)) {
            cout << "Load frame failed: " << path << " (skipping)" << endl;
            logln("[Player] Load frame failed: " + (string)path + " (skipping)");
            continue;
        }

        rgbaToAsciiBuffers(rgbaArr, w, h,
            dataBuffer, frontColor, backColor,
            CONSOLE_W, CONSOLE_H);

        printer.setData(dataBuffer, frontColor, backColor);
        printer.draw(true);  // withColor = true

        DWORD frameEndTick = GetTickCount();
        DWORD frameProcessTime = frameEndTick - frameStartTick;
        
        targetMs = (idx - startIndex + 1) * frameDuration;
        targetTick = startTick + (DWORD)(targetMs + 0.5);
        now = GetTickCount();
        
        cout << "Frame " << idx << " processed in " << frameProcessTime << "ms" << endl;
        logln("[Player] Frame " + to_string(idx) + " processed in " + to_string(frameProcessTime) + "ms");
        
        if (now < targetTick) {
            Sleep(targetTick - now);
        }
    }

    delete[] dataBuffer;
    delete[] frontColor;
    delete[] backColor;
}

int main()
{
    if (initializeLogFile()) {
        cout << "Log file created: " << g_logFilePath << endl;
        logln("Log file initialized successfully.");
    } else {
        cout << "Warning: Failed to create log file. Logging to console only." << endl;
        char currentDir[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, currentDir);
        cout << "Current directory: " << currentDir << endl;
    }
    
    if (!initializeFFmpeg()) {
        logln("\nWarning: ffmpeg.exe not found. Video file playback (option 3) will not work.");
        logln("Image display (option 1) and image sequence playback (option 2) will still work.\n");
    }
    
    cout << "\nASCII Image / Video Player\n";
    cout << "1. Show single ASCII image\n";
    cout << "2. Play ASCII video (image sequence)\n";
    cout << "3. Play video file (with auto frame extraction and adaptive FPS)\n";
    cout << "Choice: ";

    int choice = 0;
    cin >> choice;
    cin.ignore(1024, '\n');

    if (choice == 1) {
        char path[512];
        cout << "Input image path (jpg/png/bmp): ";
        cin.getline(path, 512);

        showAsciiImage(path);
    }
    else if (choice == 2) {
        char pattern[512];
        int startIdx, endIdx, fps;

        cout << "Input filename pattern (e.g. video\\frame_%04d.png): ";
        cin.getline(pattern, 512);
        cout << "Start index: ";
        cin >> startIdx;
        cout << "End index: ";
        cin >> endIdx;
        cout << "FPS: ";
        cin >> fps;
        cin.ignore(1024, '\n');

        playAsciiVideo(pattern, startIdx, endIdx, fps);

        cout << "\nVideo done. Press Enter to exit...";
        getchar();
    }
    else if (choice == 3) {
        if (g_ffmpegPath.empty()) {
            logln("ERROR: ffmpeg.exe is required for video file playback!");
            logln("Please ensure ffmpeg.exe is available and restart the program.");
            return 1;
        }
        
        char videoPath[512];
        log("Input video file path: ");
        cin.getline(videoPath, 512);
        logln(string(videoPath));
        
        const char* tempDir = "temp_frames";
        if (_access(tempDir, 0) != 0) {
            _mkdir(tempDir);
        }
        
        char framePattern[512];
        sprintf(framePattern, "%s\\frame_%%04d.png", tempDir);
        
        char audioPath[512];
        sprintf(audioPath, "%s\\audio.wav", tempDir);
        
        logln("\nStep 1: Extracting video frames...");
        if (!extractVideoFrames(videoPath, framePattern)) {
            logln("Failed to extract video frames. Make sure ffmpeg is installed and in PATH.");
            return 1;
        }
        
        logln("\nStep 2: Extracting audio...");
        if (!extractAudio(videoPath, audioPath)) {
            logln("Warning: Failed to extract audio. Video will play without sound.");
            audioPath[0] = '\0';
        }
        
        logln("\nStep 3: Calculating optimal FPS...");
        int optimalFPS = calculateOptimalFPS(framePattern, 1, 5);
        
        int frameCount = 0;
        for (int i = 1; ; ++i) {
            char testPath[512];
            sprintf(testPath, framePattern, i);
            HANDLE testFile = CreateFileA(testPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (testFile == INVALID_HANDLE_VALUE) {
                break;
            }
            CloseHandle(testFile);
            frameCount = i;
        }
        
        char logMsg[256];
        sprintf(logMsg, "\nFound %d frames.", frameCount);
        logln(logMsg);
        sprintf(logMsg, "Optimal FPS: %d", optimalFPS);
        logln(logMsg);
        logln("\nStarting playback...");
        
        if (audioPath[0] != '\0') {
            playAsciiVideoWithAudio(framePattern, 1, frameCount, optimalFPS, audioPath);
        } else {
            playAsciiVideo(framePattern, 1, frameCount, optimalFPS);
        }
        
        cout << "\nPlayback complete. Clean up temporary files? (y/n): ";
        char cleanup;
        cin >> cleanup;
        if (cleanup == 'y' || cleanup == 'Y') {
            char cleanupCmd[512];
            sprintf(cleanupCmd, "rd /s /q %s", tempDir);
            system(cleanupCmd);
            cout << "Temporary files cleaned up." << endl;
        }
        
        cout << "\nPress Enter to exit...";
        getchar();
        getchar();
    }
    else {
        logln("Invalid choice.");
    }

    closeLogFile();
    if (!g_logFilePath.empty()) {
        cout << "\nLog file saved: " << g_logFilePath << endl;
    }
    
    return 0;
}
