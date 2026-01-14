#include "CameraScannerUI.h"
#include "glog/logging.h"
#include "scanner_l/scan_io.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>

CameraScannerUI::CameraScannerUI()
    : window_(nullptr)
    , window_should_close_(false)
    , scanner_state_(ScannerState::IDLE)
    , config_path_("../ScannerConfig/")
    , data_root_path_("./scan_data/")
    , should_stop_(false)
    , show_demo_window_(false)
    , show_control_panel_(true)
    , show_status_panel_(true)
    , show_data_panel_(true)
    , imgui_cleaned_up_(false)
    , current_page_index_(0)
    , log_auto_wrap_(true)
    , log_show_info_(true)
    , log_show_warning_(true)
    , log_show_error_(true)
    , nav_window_initialized_(false)
    , content_window_initialized_(false)
{
#ifdef _WIN32
    strncpy_s(config_path_buffer_, "../ScannerConfig/", sizeof(config_path_buffer_) - 1);
#else
    strncpy(config_path_buffer_, "../ScannerConfig/", sizeof(config_path_buffer_) - 1);
#endif
    config_path_buffer_[sizeof(config_path_buffer_) - 1] = '\0';
}

CameraScannerUI::~CameraScannerUI() {
    Cleanup();
}

bool CameraScannerUI::Initialize(int width, int height, const char* title) {
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // 配置 GLFW
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // 创建窗口
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // 启用垂直同步

    // 初始化 IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    
    // 配置 IMGUI
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // 设置样式
    ImGui::StyleColorsDark();
    
    // 初始化平台/渲染器绑定
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 加载中文字体
    float baseFontSize = 18.0f;
    ImFont* font = nullptr;
    
    // 尝试多个可能的字体路径
    std::vector<std::string> font_paths = {
        std::filesystem::current_path().string() + "\\msyh.ttc",
        std::filesystem::current_path().string() + "\\assert\\msyh.ttc",
        std::filesystem::current_path().string() + "\\..\\assert\\msyh.ttc",
        std::filesystem::current_path().string() + "\\..\\..\\assert\\msyh.ttc",
        "assert\\msyh.ttc",
        "..\\assert\\msyh.ttc"
    };
    
    // 首先尝试从项目目录加载
    for (const auto& ttcfilepath : font_paths) {
        if (std::filesystem::exists(ttcfilepath)) {
            LOG(INFO) << "尝试加载字体文件: " << ttcfilepath;
            font = io.Fonts->AddFontFromFileTTF(
                ttcfilepath.c_str(),
                baseFontSize,
                nullptr,
                io.Fonts->GetGlyphRangesChineseFull()
            );
            if (font != nullptr) {
                LOG(INFO) << "字体加载成功: " << ttcfilepath;
                break;
            } else {
                LOG(WARNING) << "字体加载失败: " << ttcfilepath;
            }
        }
    }
    
    // 如果文件加载失败，尝试从 Windows 系统字体目录加载
    if (font == nullptr) {
        LOG(WARNING) << "无法从项目目录加载字体，尝试从系统字体目录加载";
        std::vector<std::string> system_font_paths = {
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/msyhbd.ttc",
            "C:/Windows/Fonts/simsun.ttc",
            "C:/Windows/Fonts/simhei.ttf"
        };
        
        for (const auto& sys_font_path : system_font_paths) {
            if (std::filesystem::exists(sys_font_path)) {
                LOG(INFO) << "尝试加载系统字体: " << sys_font_path;
                font = io.Fonts->AddFontFromFileTTF(
                    sys_font_path.c_str(),
                    baseFontSize,
                    nullptr,
                    io.Fonts->GetGlyphRangesChineseFull()
                );
                if (font != nullptr) {
                    LOG(INFO) << "系统字体加载成功: " << sys_font_path;
                    break;
                }
            }
        }
    }
    
    // 如果还是失败，使用默认字体（可能不支持中文，但至少能显示）
    if (font == nullptr) {
        LOG(ERROR) << "所有字体加载尝试都失败，使用默认字体（可能不支持中文）";
        font = io.Fonts->AddFontDefault();
    }
    
    // 设置默认字体
    if (font != nullptr) {
        io.FontDefault = font;
        LOG(INFO) << "默认字体设置成功";
    } else {
        LOG(ERROR) << "警告：默认字体未设置，可能导致中文显示异常";
    }

    // 初始化扫描器 API
    scanner_api_ = std::make_unique<ScannerLApi>();

    status_message_ = "系统就绪";
    scanner_state_ = ScannerState::IDLE;
    AddLogMessage("系统初始化完成");

    return true;
}

void CameraScannerUI::Run() {
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(window_) && !window_should_close_) {
        glfwPollEvents();

        // 开始 IMGUI 帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 渲染 UI
        RenderUI();

        // 渲染
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, 
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
    
    // 在窗口关闭前清理 ImGui（此时 OpenGL 上下文仍然有效）
    CleanupImGui();
}

void CameraScannerUI::CleanupImGui() {
    // 清理 IMGUI（在窗口关闭前，OpenGL 上下文仍然有效）
    if (!imgui_cleaned_up_ && ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imgui_cleaned_up_ = true;
    }
}

void CameraScannerUI::Cleanup() {
    should_stop_ = true;
    
    // 停止扫描器
    if (scanner_api_ && scanner_state_ != ScannerState::IDLE) {
        try {
            if (scanner_state_ == ScannerState::SCANNING) {
                StopScan();
            }
            DisconnectScanner();
        } catch (...) {
            // 忽略异常
        }
    }

    // 清理 IMGUI（如果还没有清理）
    CleanupImGui();

    // 清理 GLFW
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfwGetCurrentContext() != nullptr) {
        glfwTerminate();
    }
}

void CameraScannerUI::RenderUI() {
    // 主窗口 - 使用 DockSpace
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("相机扫描系统", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    // 提交 DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    // 菜单栏
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("窗口")) {
            ImGui::MenuItem("控制面板", nullptr, &show_control_panel_);
            ImGui::MenuItem("状态面板", nullptr, &show_status_panel_);
            ImGui::MenuItem("数据面板", nullptr, &show_data_panel_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("帮助")) {
            if (ImGui::MenuItem("关于")) {
                // 可以显示关于对话框
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // 显示导航面板和内容页面
    ShowTreeView();
    ShowMainView();

    ImGui::End();
}

void CameraScannerUI::ShowTreeView() {
    // 第一次运行时设置初始位置和大小（左侧1/5宽度）
    if (!nav_window_initialized_) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        float nav_width = viewport->WorkSize.x * 0.2f;  // 1/5 宽度
        float nav_height = viewport->WorkSize.y;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(nav_width, nav_height));
        nav_window_initialized_ = true;
    }
    
    ImGui::Begin("导航页面", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::TreeNode("功能")) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        
        if (ImGui::TreeNode((void*)(intptr_t)0, "扫描功能")) {
            if (ImGui::SmallButton("L系列线扫相机")) { 
                current_page_index_ = 0; 
            }
            if (ImGui::SmallButton("A系列面阵相机")) { 
                current_page_index_ = 0; 
            }
            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode((void*)(intptr_t)1, "结果查询")) {
            if (ImGui::SmallButton("结果表格")) { 
                current_page_index_ = 1; 
            }
            ImGui::TreePop();
        }
        
        if (ImGui::TreeNode((void*)(intptr_t)2, "图标查询")) {
            if (ImGui::SmallButton("图标查询")) { 
                current_page_index_ = 2; 
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    
    if (ImGui::TreeNode("其它")) {
        if (ImGui::SmallButton("还没想好")) { 
            // 待实现
        }
        ImGui::TreePop();
    }
    
    ImGui::End();
}

void CameraScannerUI::ShowMainView() {
    // 第一次运行时设置初始位置和大小（右侧4/5宽度）
    if (!content_window_initialized_) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        float nav_width = viewport->WorkSize.x * 0.2f;  // 1/5 宽度
        float content_width = viewport->WorkSize.x * 0.8f;  // 4/5 宽度
        float content_height = viewport->WorkSize.y;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + nav_width, viewport->WorkPos.y));
        ImGui::SetNextWindowSize(ImVec2(content_width, content_height));
        content_window_initialized_ = true;
    }
    
    ImGui::Begin("内容页面", nullptr, ImGuiWindowFlags_NoCollapse);
    
    // 根据当前页面索引显示不同内容
    switch (current_page_index_) {
        case 0:  // 扫描功能
            ShowControlPanel();
            ImGui::Separator();
            ShowStatusPanel();
            break;
        case 1:  // 结果查询
            ShowDataPanel();
            break;
        case 2:  // 图标查询
            ImGui::Text("图标查询功能待实现");
            break;
        default:
            ShowControlPanel();
            break;
    }
    
    ImGui::End();
}

void CameraScannerUI::ShowControlPanel() {
    ImGui::Text("扫描功能");
    ImGui::Separator();

    // 配置路径输入
    ImGui::Text("配置路径:");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##config_path", config_path_buffer_, sizeof(config_path_buffer_));
    ImGui::PopItemWidth();
    if (ImGui::Button("应用配置路径", ImVec2(-1, 25))) {
        config_path_ = std::string(config_path_buffer_);
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "配置路径已更新: " + config_path_;
        AddLogMessage("配置路径已更新: " + config_path_);
    }

    ImGui::Separator();

    // 状态显示
    ImGui::Text("当前状态:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, 
        scanner_state_ == ScannerState::ERROR_STATE ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) :
        scanner_state_ == ScannerState::SCANNING ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
        scanner_state_ == ScannerState::CONNECTED ? ImVec4(0.0f, 0.8f, 1.0f, 1.0f) :
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::Text("%s", GetStateText());
    ImGui::PopStyleColor();
    
    ImGui::Separator();

    // 控制按钮 - 第一行
    ImGui::BeginDisabled(scanner_state_ != ScannerState::IDLE);
    if (ImGui::Button("初始化", ImVec2(-1, 35))) {
        InitScanner();
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(scanner_state_ != ScannerState::IDLE);
    if (ImGui::Button("连接", ImVec2(-1, 35))) {
        ConnectScanner();
    }
    ImGui::EndDisabled();

    // 第二行
    ImGui::BeginDisabled(scanner_state_ != ScannerState::CONNECTED);
    if (ImGui::Button("开始扫描", ImVec2(-1, 35))) {
        StartScan();
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(scanner_state_ != ScannerState::SCANNING);
    if (ImGui::Button("停止扫描", ImVec2(-1, 35))) {
        StopScan();
    }
    ImGui::EndDisabled();

    // 第三行
    ImGui::BeginDisabled(scanner_state_ == ScannerState::IDLE);
    if (ImGui::Button("断开连接", ImVec2(-1, 35))) {
        DisconnectScanner();
    }
    ImGui::EndDisabled();
}

void CameraScannerUI::ShowStatusPanel() {
    ImGui::Text("LOG:");
    
    // LOG 控制选项
    ImGui::Checkbox("自动换行", &log_auto_wrap_);
    ImGui::SameLine();
    if (ImGui::Button("清除LOG")) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Info", &log_show_info_);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &log_show_warning_);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &log_show_error_);
    
    ImGui::Separator();
    
    // 显示日志消息
    std::lock_guard<std::mutex> lock(log_mutex_);
    ImGui::BeginChild("LogMessages", ImVec2(0, 0), true, 
        log_auto_wrap_ ? ImGuiWindowFlags_HorizontalScrollbar : ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    // 同时显示状态消息和日志消息
    {
        std::lock_guard<std::mutex> status_lock(status_mutex_);
        if (!status_message_.empty()) {
            ImGui::TextWrapped("%s", status_message_.c_str());
        }
    }
    
    for (const auto& msg : log_messages_) {
        ImGui::TextWrapped("%s", msg.c_str());
    }
    
    // 自动滚动到底部
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
}

void CameraScannerUI::AddLogMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    // 获取当前时间
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[100];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "[%H:%M:%S] ", timeinfo);
    
    log_messages_.push_back(std::string(buffer) + message);
    
    // 限制日志消息数量，避免内存过多占用
    if (log_messages_.size() > 1000) {
        log_messages_.erase(log_messages_.begin(), log_messages_.begin() + 500);
    }
}

void CameraScannerUI::ShowDataPanel() {
    ImGui::Text("扫描数据信息");
    ImGui::Separator();

    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (point_clouds_.empty()) {
        ImGui::Text("暂无数据");
    } else {
        ImGui::Text("相机数量: %zu", point_clouds_.size());
        
        for (size_t i = 0; i < point_clouds_.size(); ++i) {
            ImGui::Text("相机 %zu:", i);
            ImGui::Indent();
            ImGui::Text("  点云数量: %zu", point_clouds_[i].size());
            ImGui::Text("  灰度图像大小: %zu", gray_images_[i].size());
            ImGui::Text("  编码器值数量: %zu", encoder_values_[i].size());
            ImGui::Text("  帧计数数量: %zu", frame_counts_[i].size());
            ImGui::Unindent();
        }
    }
}

void CameraScannerUI::InitScanner() {
    if (scanner_state_ != ScannerState::IDLE) {
        return;
    }

    scanner_state_ = ScannerState::INITIALIZING;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "正在初始化扫描器...";
        AddLogMessage("正在初始化扫描器...");
    }

    // 在后台线程中执行初始化
    std::thread([this]() {
        try {
            // 读取配置文件获取 data_root_path
            std::string path_config_file = config_path_ + "../all_path_config.json";
            // 如果 config_path_ 是相对路径，尝试多个可能的路径
            if (!std::filesystem::exists(path_config_file)) {
                path_config_file = config_path_ + "all_path_config.json";
            }
            if (!std::filesystem::exists(path_config_file)) {
                path_config_file = "../ScannerConfig/all_path_config.json";
            }
            
            if (std::filesystem::exists(path_config_file)) {
                std::ifstream f(path_config_file);
                if (f.is_open()) {
                    nlohmann::json data = nlohmann::json::parse(f);
                    f.close();
                    data_root_path_ = data["data_root_path"];
                    LOG(INFO) << "读取 data_root_path: " << data_root_path_;
                } else {
                    LOG(WARNING) << "无法打开配置文件: " << path_config_file;
                    data_root_path_ = "./scan_data/";  // 默认路径
                }
            } else {
                LOG(WARNING) << "配置文件不存在: " << path_config_file << "，使用默认路径";
                data_root_path_ = "./scan_data/";  // 默认路径
            }
            
            // 确保路径以 / 或 \ 结尾
            if (!data_root_path_.empty() && data_root_path_.back() != '/' && data_root_path_.back() != '\\') {
                data_root_path_ += "/";
            }
            
            scanner_api_->SetConfigRootPath(config_path_);
            int result = scanner_api_->Init();
            
            if (result == 0) {
                scanner_state_ = ScannerState::IDLE;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "初始化成功，数据保存路径: " + data_root_path_;
                AddLogMessage("初始化成功，数据保存路径: " + data_root_path_);
            } else {
                scanner_state_ = ScannerState::ERROR_STATE;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "初始化失败，错误代码: " + std::to_string(result);
                AddLogMessage("初始化失败，错误代码: " + std::to_string(result));
            }
        } catch (const std::exception& e) {
            scanner_state_ = ScannerState::ERROR_STATE;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "初始化异常: " + std::string(e.what());
            AddLogMessage("初始化异常: " + std::string(e.what()));
        }
    }).detach();
}

void CameraScannerUI::ConnectScanner() {
    if (scanner_state_ != ScannerState::IDLE) {
        return;
    }

    scanner_state_ = ScannerState::CONNECTING;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "正在连接扫描器...";
    }

    std::thread([this]() {
        try {
            int result = scanner_api_->Connect();
            
            if (result == 0) {
                scanner_state_ = ScannerState::CONNECTED;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "连接成功";
            } else {
                scanner_state_ = ScannerState::ERROR_STATE;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "连接失败，错误代码: " + std::to_string(result);
            }
        } catch (const std::exception& e) {
            scanner_state_ = ScannerState::ERROR_STATE;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "连接异常: " + std::string(e.what());
        }
    }).detach();
}

void CameraScannerUI::StartScan() {
    if (scanner_state_ != ScannerState::CONNECTED) {
        return;
    }

    scanner_state_ = ScannerState::SCANNING;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "开始扫描...";
    }

    std::thread([this]() {
        try {
            int result = scanner_api_->Start();
            
            if (result == 0) {
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "扫描进行中...";
            } else {
                scanner_state_ = ScannerState::ERROR_STATE;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "启动扫描失败，错误代码: " + std::to_string(result);
            }
        } catch (const std::exception& e) {
            scanner_state_ = ScannerState::ERROR_STATE;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "启动扫描异常: " + std::string(e.what());
        }
    }).detach();
}

void CameraScannerUI::StopScan() {
    if (scanner_state_ != ScannerState::SCANNING) {
        return;
    }

    scanner_state_ = ScannerState::STOPPING;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "正在停止扫描...";
    }

    std::thread([this]() {
        try {
            // 停止扫描
            int result = scanner_api_->End();
            
            if (result == 0) {
                {
                    std::lock_guard<std::mutex> lock(status_mutex_);
                    status_message_ = "扫描已停止，正在获取数据...";
                }
                
                // 自动获取数据
                std::vector<std::vector<cv::Point3f>> pc_vec;
                std::vector<std::vector<uint8_t>> gray_vec;
                std::vector<std::vector<int32_t>> encoder_vec;
                std::vector<std::vector<uint32_t>> framecnt_vec;

                int data_result = scanner_api_->GetAllData(pc_vec, gray_vec, encoder_vec, framecnt_vec);
                
                if (data_result == 0) {
                    // 先保存数据到文件（使用临时变量）
                    SaveScanData(pc_vec, gray_vec, encoder_vec, framecnt_vec);
                    
                    // 然后保存数据到成员变量
                    {
                        std::lock_guard<std::mutex> lock(data_mutex_);
                        point_clouds_ = std::move(pc_vec);
                        gray_images_ = std::move(gray_vec);
                        encoder_values_ = std::move(encoder_vec);
                        frame_counts_ = std::move(framecnt_vec);
                    }
                    
                    scanner_state_ = ScannerState::CONNECTED;
                    std::lock_guard<std::mutex> lock(status_mutex_);
                    status_message_ = "扫描已停止，数据已获取并保存";
                } else {
                    scanner_state_ = ScannerState::CONNECTED;
                    std::lock_guard<std::mutex> lock(status_mutex_);
                    status_message_ = "扫描已停止，但获取数据失败，错误代码: " + std::to_string(data_result);
                }
            } else {
                scanner_state_ = ScannerState::ERROR_STATE;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "停止扫描失败，错误代码: " + std::to_string(result);
            }
        } catch (const std::exception& e) {
            scanner_state_ = ScannerState::ERROR_STATE;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "停止扫描异常: " + std::string(e.what());
        }
    }).detach();
}

void CameraScannerUI::DisconnectScanner() {
    if (scanner_state_ == ScannerState::IDLE) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "正在断开连接...";
    }

    std::thread([this]() {
        try {
            if (scanner_state_ == ScannerState::SCANNING) {
                scanner_api_->End();
            }
            
            int result = scanner_api_->disconnect();
            
            scanner_state_ = ScannerState::IDLE;
            std::lock_guard<std::mutex> lock(status_mutex_);
            if (result == 0) {
                status_message_ = "已断开连接";
            } else {
                status_message_ = "断开连接完成（可能有警告）";
            }
        } catch (const std::exception& e) {
            scanner_state_ = ScannerState::ERROR_STATE;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "断开连接异常: " + std::string(e.what());
        }
    }).detach();
}

// 辅助函数：保存点云为 TIFF
static cv::Mat save_ply2tiff(std::vector<float>& x_coords, std::vector<float>& y_coords, std::vector<float>& z_coords,
    int width, int height, const std::string& filename) {
    cv::Mat tiff_image(height, width, CV_32FC3);
    
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int idx = row * width + col;
            if (idx < static_cast<int>(x_coords.size())) {
                cv::Vec3f& pixel = tiff_image.at<cv::Vec3f>(row, col);
                pixel[0] = x_coords[idx];  // X
                pixel[1] = y_coords[idx];  // Y
                pixel[2] = z_coords[idx];  // Z
            }
        }
    }
    // 保存TIFF文件
    std::vector<int> compression_params = { cv::IMWRITE_TIFF_COMPRESSION, 1 };
    cv::imwrite(filename, tiff_image, compression_params);
    return tiff_image;
}

// 辅助函数：保存灰度为 TIFF
static cv::Mat save_gray2tiff(std::vector<uint8_t>& data, int width, int height, const std::string& filename) {
    cv::Mat tiff_image(height, width, CV_8UC1, data.data());
    
    // 保存TIFF文件
    std::vector<int> compression_params = { cv::IMWRITE_TIFF_COMPRESSION, 1 };
    cv::imwrite(filename, tiff_image, compression_params);
    return tiff_image;
}

void CameraScannerUI::SaveScanData(const std::vector<std::vector<cv::Point3f>>& pc_vec,
                                    const std::vector<std::vector<uint8_t>>& gray_vec,
                                    const std::vector<std::vector<int32_t>>& encoder_vec,
                                    const std::vector<std::vector<uint32_t>>& framecnt_vec) {
    try {
        // 获取当前时间作为文件名
        time_t rawtime;
        struct tm* timeinfo;
        char buffer[100];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeinfo);
        std::string date_time_str = buffer;
        
        // 使用配置的 data_root_path
        std::string save_dir = data_root_path_.empty() ? "./scan_data/" : data_root_path_;
        
        // 确保路径以 / 或 \ 结尾
        if (!save_dir.empty() && save_dir.back() != '/' && save_dir.back() != '\\') {
            save_dir += "/";
        }
        
        // 创建保存目录
        std::filesystem::create_directories(save_dir);
        
        LOG(INFO) << "开始保存数据，保存路径: " << save_dir;
        LOG(INFO) << "相机数量: " << pc_vec.size();
        
        // 保存每个相机的数据
        for (size_t j = 0; j < pc_vec.size(); ++j) {
            if (pc_vec[j].empty()) {
                LOG(WARNING) << "相机 " << j << " 数据为空，跳过";
                continue;
            }
            
            // 构建文件路径
            std::string path_laser_scan_pc = save_dir + "pointclouds_loop_" + date_time_str + "_scan_" + std::to_string(j) + ".ply";
            std::string path_laser_scan_tiff_pc = save_dir + "pointclouds_loop_" + date_time_str + "_scan_" + std::to_string(j) + "_pc.tiff";
            std::string path_laser_scan_tiff_gray = save_dir + "pointclouds_loop_" + date_time_str + "_scan_" + std::to_string(j) + "_gray.tiff";
            
            LOG(INFO) << "保存相机 " << j << " 数据:";
            LOG(INFO) << "  点云数量: " << pc_vec[j].size();
            LOG(INFO) << "  灰度图像大小: " << gray_vec[j].size();
            LOG(INFO) << "  编码器值数量: " << encoder_vec[j].size();
            LOG(INFO) << "  帧计数数量: " << framecnt_vec[j].size();
            
            // 检查数据大小是否匹配
            if (pc_vec[j].size() != gray_vec[j].size() || 
                pc_vec[j].size() != encoder_vec[j].size() || 
                pc_vec[j].size() != framecnt_vec[j].size()) {
                LOG(ERROR) << "相机 " << j << " 数据大小不匹配，跳过保存";
                continue;
            }
            
            // 提取 x, y, z 坐标
            std::vector<float> i_pc_x, i_pc_y, i_pc_z;
            i_pc_x.reserve(pc_vec[j].size());
            i_pc_y.reserve(pc_vec[j].size());
            i_pc_z.reserve(pc_vec[j].size());
            
            for (size_t k = 0; k < pc_vec[j].size(); ++k) {
                i_pc_x.push_back(pc_vec[j][k].x);
                i_pc_y.push_back(pc_vec[j][k].y);
                i_pc_z.push_back(pc_vec[j][k].z);
            }
            
            // 计算图像尺寸（假设宽度为 3200）
            int data_width = 3200;
            int data_height = static_cast<int>(i_pc_z.size() / data_width);
            if (data_height == 0) {
                LOG(WARNING) << "相机 " << j << " 数据高度为 0，使用默认高度 1";
                data_height = 1;
            }
            
            // 转换编码器和帧计数类型
            std::vector<int> encoder_vec_int;
            std::vector<unsigned int> framecnt_vec_uint;
            encoder_vec_int.reserve(encoder_vec[j].size());
            framecnt_vec_uint.reserve(framecnt_vec[j].size());
            
            for (size_t k = 0; k < encoder_vec[j].size(); ++k) {
                encoder_vec_int.push_back(static_cast<int>(encoder_vec[j][k]));
            }
            for (size_t k = 0; k < framecnt_vec[j].size(); ++k) {
                framecnt_vec_uint.push_back(static_cast<unsigned int>(framecnt_vec[j][k]));
            }
            
            // 保存 PLY 文件
            m_PlyFormat format = m_PlyFormat::BINARY;
            bool ply_status = WritePCToPLY(
                pc_vec[j].data(), 
                static_cast<int>(pc_vec[j].size()), 
                path_laser_scan_pc,
                nullptr, 0,
                encoder_vec_int.data(), 
                static_cast<int>(encoder_vec_int.size()),
                framecnt_vec_uint.data(), 
                static_cast<int>(framecnt_vec_uint.size()),
                gray_vec[j].data(), 
                static_cast<int>(gray_vec[j].size())
            );
            
            if (ply_status) {
                LOG(INFO) << "PLY 文件保存成功: " << path_laser_scan_pc;
            } else {
                LOG(ERROR) << "PLY 文件保存失败: " << path_laser_scan_pc;
            }
            
            // 保存 TIFF 文件（点云）
            if (i_pc_x.size() > 0 && i_pc_y.size() > 0 && i_pc_z.size() > 0) {
                cv::Mat tiff_image = save_ply2tiff(i_pc_x, i_pc_y, i_pc_z, data_width, data_height, path_laser_scan_tiff_pc);
                LOG(INFO) << "点云 TIFF 文件保存成功: " << path_laser_scan_tiff_pc;
            }
            
            // 保存 TIFF 文件（灰度）
            if (gray_vec[j].size() > 0) {
                // 创建灰度数据的副本（因为 save_gray2tiff 需要非 const 引用）
                std::vector<uint8_t> gray_data_copy = gray_vec[j];
                cv::Mat tiff_image_gray = save_gray2tiff(
                    gray_data_copy, 
                    data_width, 
                    data_height, 
                    path_laser_scan_tiff_gray
                );
                LOG(INFO) << "灰度 TIFF 文件保存成功: " << path_laser_scan_tiff_gray;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "数据已保存到: " + save_dir;
        }
        LOG(INFO) << "所有数据保存完成，保存路径: " << save_dir;
    } catch (const std::exception& e) {
        LOG(ERROR) << "保存数据异常: " << e.what();
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "保存数据异常: " + std::string(e.what());
    }
}

const char* CameraScannerUI::GetStateText() const {
    switch (scanner_state_) {
        case ScannerState::IDLE: return "空闲";
        case ScannerState::INITIALIZING: return "初始化中";
        case ScannerState::CONNECTING: return "连接中";
        case ScannerState::CONNECTED: return "已连接";
        case ScannerState::SCANNING: return "扫描中";
        case ScannerState::STOPPING: return "停止中";
        case ScannerState::ERROR_STATE: return "错误";
        default: return "未知";
    }
}
