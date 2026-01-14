#ifndef CAMERA_SCANNER_UI_H
#define CAMERA_SCANNER_UI_H

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "scanner_l/scanner_l_api.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// 扫描器状态
enum class ScannerState {
    IDLE,           // 空闲
    INITIALIZING,   // 初始化中
    CONNECTING,     // 连接中
    CONNECTED,      // 已连接
    SCANNING,       // 扫描中
    STOPPING,       // 停止中
    ERROR_STATE     // 错误
};

class CameraScannerUI {
public:
    CameraScannerUI();
    ~CameraScannerUI();

    // 初始化 GLFW 和 IMGUI
    bool Initialize(int width = 1280, int height = 720, const char* title = "相机扫描系统");
    
    // 主循环
    void Run();
    
    // 清理资源
    void Cleanup();
    
    // 清理 ImGui（内部方法，在窗口关闭前调用）
    void CleanupImGui();

private:
    // 渲染 UI
    void RenderUI();
    
    // 显示导航面板
    void ShowTreeView();
    
    // 显示内容页面
    void ShowMainView();
    
    // 显示控制面板
    void ShowControlPanel();
    
    // 显示状态信息
    void ShowStatusPanel();
    
    // 显示数据信息
    void ShowDataPanel();
    
    // 扫描器操作（异步）
    void InitScanner();
    void ConnectScanner();
    void StartScan();
    void StopScan();
    void DisconnectScanner();
    
    // 保存扫描数据（内部方法）
    void SaveScanData(const std::vector<std::vector<cv::Point3f>>& pc_vec,
                      const std::vector<std::vector<uint8_t>>& gray_vec,
                      const std::vector<std::vector<int32_t>>& encoder_vec,
                      const std::vector<std::vector<uint32_t>>& framecnt_vec);

    // 更新状态文本
    const char* GetStateText() const;
    
    // 添加日志消息（内部方法）
    void AddLogMessage(const std::string& message);

private:
    GLFWwindow* window_;
    bool window_should_close_;
    
    // 扫描器 API
    std::unique_ptr<ScannerLApi> scanner_api_;
    ScannerState scanner_state_;
    std::string config_path_;
    std::string data_root_path_;  // 数据保存根路径
    
    // 状态信息
    std::string status_message_;
    std::mutex status_mutex_;
    
    // 数据信息
    std::vector<std::vector<cv::Point3f>> point_clouds_;
    std::vector<std::vector<uint8_t>> gray_images_;
    std::vector<std::vector<int32_t>> encoder_values_;
    std::vector<std::vector<uint32_t>> frame_counts_;
    std::mutex data_mutex_;
    
    // 线程控制
    std::atomic<bool> should_stop_;
    std::thread scanner_thread_;
    
    // UI 状态
    bool show_demo_window_;
    bool show_control_panel_;
    bool show_status_panel_;
    bool show_data_panel_;
    
    // 配置路径输入
    char config_path_buffer_[512];
    
    // 清理状态标志
    bool imgui_cleaned_up_;
    
    // 导航页面索引
    int current_page_index_;  // 0: 扫描功能, 1: 结果查询, 2: 图标查询
    
    // LOG 显示选项
    bool log_auto_wrap_;
    bool log_show_info_;
    bool log_show_warning_;
    bool log_show_error_;
    std::vector<std::string> log_messages_;
    std::mutex log_mutex_;
    
    // 布局初始化标志
    bool nav_window_initialized_;
    bool content_window_initialized_;
};

#endif // CAMERA_SCANNER_UI_H
