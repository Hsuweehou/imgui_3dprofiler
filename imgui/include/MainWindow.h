#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include "BaseWindow.h"
#include <windows.h>
#include <filesystem>
#include <imgui_internal.h>
#include "scanner_l/scanner_l_api.h"
 //#include "scanner_l/scan_share_memory.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include  <nlohmann/json.hpp>
#include "../../plc_serial/include/mitsubishi_plc_fx_link.h"
#include "scanner_l/Scanner_Server.h"
#include "utility.h"
#include "glog/logging.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>

#define SPLAT_WINDOWS 0

// 扫描器状态枚举
enum class ScannerStatus {
    IDLE,           // 空闲
    INITIALIZING,   // 初始化中
    CONNECTING,     // 连接中
    CONNECTED,      // 已连接
    RESETTING,      // 复位中
    STARTING,       // 启动中
    SCANNING,       // 扫描中
    STOPPING,       // 停止中
    DISCONNECTING,  // 断开连接中
    ERROR_SCANNER   // 错误
};

class DLLEXPORT MainWindow
{
public:
    // 初始化
    MainWindow(const int &_winrow, const int &_wincol);

    // 析构，清理内存
    ~MainWindow();

    // 初始化 opengl 和glfw
    void init();

    //程序是否退出
    bool GetMainShouldClose();

    //主UI函数，放停靠空间的代码
    void RenderPrepare();

    void RenderUI();

    //隐藏窗口的TabBar
    void HideTabBar();
    //导航页面
    void ShowTreeView();
    //内容页面
    void ShowMainView();
    /*
    * 内容页面0-5
    */
    void ShowPageView0();
    void ShowPageLSerialScanner();
    void ShowPageView2();
    void ShowPageASerialScanner();

    // 扫描器操作函数（线程安全）
    void ScannerInitAsync(const std::string& config_path);
    void ScannerConnectAsync();
    void ScannerResetAsync();
    void ScannerStartAsync();
    void ScannerEndAsync();
    void ScannerDisconnectAsync();
    
    // 获取当前状态
    ScannerStatus GetScannerStatus() const;
    std::string GetScannerStatusString() const;

    ImVec4 GetSeverityColor(google::LogSeverity severity) {
        switch (severity) {
        case google::GLOG_INFO: return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        case google::GLOG_WARNING: return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        case google::GLOG_ERROR: return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        case google::GLOG_FATAL: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    // 释放资源
    void release();

public:
    //一级索引
    int FirstIdx = 0;
    //二级索引
    int SecondIdx = 0;
    /*AsyncLogBuffer log_buffer;
    ImGuiLogSink log_sink(log_buffer);*/
private:
    bool MainOpen = true;
    // 日志窗口
    RealtimeLogSink log_sink;
    bool auto_scroll = true;
    bool show_info = true, show_warning = true, show_error = true;
    bool need_scroll = false;
    // 窗口信息
    int winrow, wincol;
    ImGuiWindowFlags window_flags;
    GLFWwindow *window = nullptr;
    ImGuiIO* _io;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 扫描器相关
    std::unique_ptr<ScannerLApi> scanner_api_;
    std::atomic<ScannerStatus> scanner_status_{ScannerStatus::IDLE};
    std::mutex scanner_mutex_;
    std::string scanner_config_path_ = "../ScannerConfig/";
    
    // 线程管理
    std::vector<std::thread> scanner_threads_;
    void CleanupScannerThreads();
};

#endif