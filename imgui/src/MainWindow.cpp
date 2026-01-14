#include "MainWindow.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include "implot/implot.h"
#include <vector>
#include <algorithm>
#include <thread>
#include "font_msyh.c"
#include "../include/IconsFontAwesome6.h"

MainWindow::MainWindow(const int &_winrow, const int &_wincol) : winrow(_winrow), wincol(_wincol)
{
    // 初始化glog
    google::InitGoogleLogging("async_glog");
    FLAGS_logtostderr = 0;
    google::AddLogSink(&log_sink);

    // 初始化扫描器API
    scanner_api_ = std::make_unique<ScannerLApi>();
    scanner_status_ = ScannerStatus::IDLE;

    // 初始化
    init();

    // 渲染
    RenderPrepare();
}

MainWindow::~MainWindow()
{
    // 清理扫描器线程
    CleanupScannerThreads();
    
    // 断开扫描器连接
    if (scanner_api_ && scanner_status_ != ScannerStatus::IDLE) {
        try {
            scanner_api_->disconnect();
        } catch (...) {
            // 忽略异常
        }
    }
    
    // 释放资源
    release();
}

bool MainWindow::GetMainShouldClose()
{
    return !MainOpen;
}


void MainWindow::init()
{
    if (!glfwInit())
    {
        throw std::runtime_error("GLFW initialization failed");
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    //glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(wincol, winrow, "相机服务程序", NULL, NULL);
    if (!window)
    {
        std::cerr << "GLFW window creation failed" << std::endl;
        glfwTerminate();
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    //ImPlot::CreateContext();
    /*ImGuiIO& io = ImGui::GetIO();
    (void)io;*/
    _io = &ImGui::GetIO(); (void)_io;
    ImGuiIO& io = *_io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Load Fonts - 尝试多个路径加载中文字体
    float baseFontSize = 30.0f;
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
    
    // 首先尝试从文件加载
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
         
    // FontAwesome字体需要缩小2.0f/3.0f才能正确对齐
    float iconFontSize = baseFontSize * 2.0f / 3.0f;     
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    
    // 尝试加载 FontAwesome 图标字体
    std::vector<std::string> icon_font_paths = {
        std::filesystem::current_path().string() + "\\assert\\fa-solid-900.ttf",
        std::filesystem::current_path().string() + "\\..\\assert\\fa-solid-900.ttf",
        std::filesystem::current_path().string() + "\\..\\..\\assert\\fa-solid-900.ttf",
        "assert\\fa-solid-900.ttf",
        "..\\assert\\fa-solid-900.ttf"
    };
    
    bool icon_font_loaded = false;
    for (const auto& icon_path : icon_font_paths) {
        if (std::filesystem::exists(icon_path)) {
            io.Fonts->AddFontFromFileTTF(icon_path.c_str(), iconFontSize, &icons_config, icons_ranges);
            icon_font_loaded = true;
            LOG(INFO) << "图标字体加载成功: " << icon_path;
            break;
        }
    }
    
    if (!icon_font_loaded) {
        // 尝试使用定义的路径
        if (std::filesystem::exists(FONT_ICON_FILE_NAME_FAS)) {
            io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_FAS, iconFontSize, &icons_config, icons_ranges);
            LOG(INFO) << "图标字体加载成功: " << FONT_ICON_FILE_NAME_FAS;
        } else {
            LOG(WARNING) << "图标字体文件未找到: " << FONT_ICON_FILE_NAME_FAS;
        }
    }
    
    // 注意：字体纹理会在第一次渲染时由 ImGui_ImplOpenGL3_NewFrame() 自动构建
    // 不需要手动构建，ImGui 会自动处理

    // Our state
    //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

}

void MainWindow::RenderPrepare()
{
    while (!glfwWindowShouldClose(window))
    {
      glfwPollEvents();
      // 检查新日志
      if (log_sink.CheckNewLogs()) {
          need_scroll = auto_scroll;
      }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
               
        RenderUI();
        if (GetMainShouldClose())
        {
            //关闭窗口，也可以做一些退出确认操作
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        //_io = &ImGui::GetIO(); (void)_io;
        //ImGuiIO& io = *_io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
        //io.ConfigViewportsNoAutoMerge = true;
        if (_io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
}

void MainWindow::RenderUI()
{
    bool* p_open = &MainOpen;
    static bool opt_fullscreen = false;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    //去掉窗口标题栏前面的折叠三角号
    static bool no_collapse = true;       

    window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    else
    {
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;


    //不允许折叠主窗口
    if (no_collapse)
        window_flags |= ImGuiWindowFlags_NoCollapse;

    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    //设置最大化、最小化
    static bool IsMaximized = false;
    static bool LastMaximized = false;
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    if (IsMaximized)
    {
        if (LastMaximized != IsMaximized)
        {
            LastMaximized = IsMaximized;
        }
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(screenSize);
        //隐藏标题栏
        // window_flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }
    else
    {
        if (LastMaximized != IsMaximized)
        {
            LastMaximized = IsMaximized;
            ImGui::SetNextWindowPos(ImVec2(screenSize[0]*0.1, screenSize[1] * 0.1));
            ImGui::SetNextWindowSize(ImVec2(screenSize[0] * 0.8, screenSize[1] * 0.8));
        }
    }


    ImGui::Begin(ICON_FA_CAMERA " 相机服务程序", p_open, window_flags);
    if (!opt_padding)
        ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // Submit the DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    else
    {
        //不关闭Docking
        //ShowDockingDisabledMessage();
    }

    if (ImGui::BeginMenuBar())
    {
        //菜单做一些汉化
        if (ImGui::BeginMenu(ICON_FA_BAHAI " 选项(Options)"))//选项（Options）string_to_utf8("显示中文").c_str()
        {
            //ImGui::MenuItem("全屏(Fullscreen)", NULL, &opt_fullscreen);
            ImGui::MenuItem("填充(Padding)", NULL, &opt_padding);

            //不关闭菜单
        /* if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
                *p_open = false;*/
            ImGui::EndMenu();
        }
        //增加主题切换
        if (ImGui::BeginMenu(ICON_FA_THERMOMETER " 主题(Styles)"))
        {
            if (ImGui::MenuItem("暗黑(Dark)")) { ImGui::StyleColorsDark(); }
            if (ImGui::MenuItem("明亮(Light)")) { ImGui::StyleColorsLight(); }

            ImGui::EndMenu();
        }
        //增加窗口切换
        if (ImGui::BeginMenu(ICON_FA_TV " 窗口(Windows)"))
        {
            if (ImGui::MenuItem(ICON_FA_WINDOW_MAXIMIZE "最大(Max)")) { IsMaximized = true; }
            if (ImGui::MenuItem(ICON_FA_WINDOW_RESTORE "默认(Default)")) { IsMaximized = false; }

            ImGui::EndMenu();
        }
        //增加帮助切换
        if (ImGui::BeginMenu(ICON_FA_TOOLBOX " 帮助(Helps)"))
        {
            if (ImGui::MenuItem(ICON_FA_FACE_KISS_WINK_HEART "关于我们")) { IsMaximized = true; }

            ImGui::EndMenu();
        }

        //HelpMarker 不需要
        ImGui::EndMenuBar();
    }

    /**添加自己的窗口**/
    ShowTreeView();
    ShowMainView();

    ImGui::End();
}

void MainWindow::HideTabBar()
{
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    //window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);
}

void MainWindow::ShowTreeView()
{
    HideTabBar();
    ImGui::Begin(ICON_FA_LIST "导航页面",nullptr, ImGuiWindowFlags_NoCollapse);
    if (ImGui::TreeNode(ICON_FA_ERASER "功能"))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        if (ImGui::TreeNode((void*)(intptr_t)0, ICON_FA_EYE "扫描功能", 0)) {
            ImGui::Text(ICON_FA_CAMERA_RETRO);
            ImGui::SameLine();
            if (ImGui::SmallButton("L系列线扫相机")) { FirstIdx = 0; SecondIdx = 0; }
            ImGui::Text(ICON_FA_CAMERA_RETRO);
            ImGui::SameLine();
            if (ImGui::SmallButton("A系列面阵相机")) { FirstIdx = 1; SecondIdx = 0; }
            ImGui::TreePop();
        }


        if (ImGui::TreeNode((void*)(intptr_t)1, ICON_FA_EYE "结果查询", 1)) {
            ImGui::Text(ICON_FA_BOOK_OPEN);
            ImGui::SameLine();
            if (ImGui::SmallButton("结果表格")) { FirstIdx = 2; SecondIdx = 0; }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode((void*)(intptr_t)2, ICON_FA_EYE "图标查询", 2)) {
            ImGui::Text(ICON_FA_BINOCULARS);
            ImGui::SameLine();
            if (ImGui::SmallButton("图标查询")) { FirstIdx = 3; SecondIdx = 0; }
            ImGui::TreePop();
        }
        ImGui::TreePop();

    }
    if (ImGui::TreeNode(ICON_FA_EXPLOSION "其它"))
    {
        ImGui::Text(ICON_FA_FACE_FLUSHED);
        ImGui::SameLine();
        if (ImGui::SmallButton("还没想好")) { }            
        ImGui::TreePop();
    }
    ImGui::End();
}

void MainWindow::ShowMainView()
{
    HideTabBar();
    // 清除之前的内容
    ImGui::Begin(ICON_FA_SHEET_PLASTIC "内容页面", nullptr, ImGuiWindowFlags_NoCollapse);
    switch (FirstIdx)
    {
    case 0:
        ShowPageLSerialScanner();
        break;
    case 1:
        ShowPageASerialScanner();
        break;
    case 2:
        ShowPageView0();
        break;
    case 3:
        ShowPageView2();
        break;
    default:
        break;
    }
    ImGui::End();
}

void MainWindow::ShowPageView0()
{
    ImGui::Text("检测结果", FirstIdx,SecondIdx);
    //一个表格示例
    static ImGuiTableFlags flags =
        ImGuiTableFlags_SizingFixedFit |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable;
    if (ImGui::BeginTable("table0", 3, flags))
    {
        ImGui::TableSetupColumn("AAA", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("BBB", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("CCC", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (int row = 0; row < 5; row++)
        {
            ImGui::TableNextRow();
            for (int column = 0; column < 3; column++)
            {
                ImGui::TableSetColumnIndex(column);
                ImGui::Text("%s %d,%d", (column == 2) ? "Stretch" : "Fixed", column, row);
            }
        }
        ImGui::EndTable();
    }
}

void MainWindow::ShowPageLSerialScanner()
{
    ImGui::Text("扫描功能", FirstIdx, SecondIdx);
    
    bool* p_open = &MainOpen;
    static bool opt_fullscreen = false;
    static bool opt_padding = false;
    
    // 显示扫描器状态
    ImGui::Separator();
    ImGui::Text("扫描器状态: ");
    ImGui::SameLine();
    ScannerStatus status = GetScannerStatus();
    ImVec4 status_color = (status == ScannerStatus::ERROR_SCANNER) ? 
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : 
        (status == ScannerStatus::SCANNING) ? 
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : 
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::TextColored(status_color, "%s", GetScannerStatusString().c_str());
    ImGui::Separator();
    
    //选项卡示例
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("单相机扫描"))
        {
            // 扫描器控制按钮
            ImGui::Text("扫描器控制:");
            ImGui::Spacing();
            
            // 初始化按钮
            if (ImGui::Button("初始化扫描器")) {
                ScannerInitAsync("../ScannerConfig/");
            }
            ImGui::SameLine();
            
            // 连接按钮
            bool can_connect = (status == ScannerStatus::IDLE || status == ScannerStatus::ERROR_SCANNER);
            if (!can_connect) ImGui::BeginDisabled();
            if (ImGui::Button("连接")) {
                ScannerConnectAsync();
            }
            if (!can_connect) ImGui::EndDisabled();
            ImGui::SameLine();
            
            // 复位按钮
            bool can_reset = (status == ScannerStatus::CONNECTED);
            if (!can_reset) ImGui::BeginDisabled();
            if (ImGui::Button("复位")) {
                ScannerResetAsync();
            }
            if (!can_reset) ImGui::EndDisabled();
            ImGui::SameLine();
            
            // 断开连接按钮
            bool can_disconnect = (status == ScannerStatus::CONNECTED || 
                                   status == ScannerStatus::SCANNING ||
                                   status == ScannerStatus::ERROR_SCANNER);
            if (!can_disconnect) ImGui::BeginDisabled();
            if (ImGui::Button("断开连接")) {
                ScannerDisconnectAsync();
            }
            if (!can_disconnect) ImGui::EndDisabled();
            
            ImGui::Separator();
            
            static char scan_nb[64] = "1";
            static char text_hint[64] = "请输入扫描数据长度!";
            // 通过添加文本框的内边距，来实现控制ImVec2(左右, 上下);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
            // 设置圆角
            //ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            // 设置输入框的宽度为 200 像素
            ImGui::PushItemWidth(200);
            ImGui::Text("扫描长度(ms): ");
            ImGui::SameLine();
            ImGui::InputTextWithHint(" ", text_hint, scan_nb, IM_ARRAYSIZE(scan_nb));
            //ImGui::InputText(" ", scan_nb, IM_ARRAYSIZE(scan_nb), ImGuiInputTextFlags_None, 0, scan_nb);
            // 恢复默认宽度
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            /*LOG(INFO) << "scan_nb: " << scan_nb << "\n";
            LOG(INFO) << "scan_nb: " << (scan_nb[0] == '\0') << "\n";*/
            //默认是1
            int scan_times = 1;
            if(scan_nb[0] != '\0')
                scan_times = std::stoi(scan_nb);
            
            ImGui::SameLine();
            
            // 开始扫描按钮
            bool can_start = (status == ScannerStatus::CONNECTED);
            if (!can_start) ImGui::BeginDisabled();
            bool scan_button_pressed = ImGui::Button("开始扫描");
            if (!can_start) ImGui::EndDisabled();
            
            // 停止扫描按钮
            ImGui::SameLine();
            bool can_stop = (status == ScannerStatus::SCANNING);
            if (!can_stop) ImGui::BeginDisabled();
            if (ImGui::Button("停止扫描")) {
                ScannerEndAsync();
            }
            if (!can_stop) ImGui::EndDisabled();
            
            if (scan_button_pressed && can_start) {
                ScannerStartAsync();
            }

            ImGui::Separator();
            ////文本显示
            ImGui::Text("LOG: ");
#if SPLAT_WINDOWS
            // 日志窗口
            ImGui::Begin("Log Viewer", p_open, window_flags);
            if (!opt_padding)
                ImGui::PopStyleVar();

            if (opt_fullscreen)
                ImGui::PopStyleVar(2);
#endif
            ImGui::Checkbox("自动换行", &auto_scroll);
            ImGui::SameLine();

            if (ImGui::Button("清除LOG")) {
                std::lock_guard<std::mutex> lock(log_sink.buffer_mutex_);
                log_sink.log_buffer_.clear();
            }

            ImGui::SameLine();
            ImGui::Checkbox("Info", &show_info);
            ImGui::SameLine();
            ImGui::Checkbox("Warning", &show_warning);
            ImGui::SameLine();
            ImGui::Checkbox("Error", &show_error);

            ImGui::Separator();
            ImGui::BeginChild("LogContent");
            auto logs = log_sink.GetLogs();
            for (auto& entry : logs) {
                if (!show_info && entry.severity == google::GLOG_INFO) continue;
                if (!show_warning && entry.severity == google::GLOG_WARNING) continue;
                if (!show_error && entry.severity >= google::GLOG_ERROR) continue;

                ImGui::TextColored(GetSeverityColor(entry.severity), "[%s]", entry.timestamp.c_str());
                ImGui::SameLine();
                ImGui::TextWrapped("%s", entry.message.c_str());
            }

            if (need_scroll) {
                ImGui::SetScrollHereY(1.0f);
                need_scroll = false;
            }

            ImGui::EndChild();
#if SPLAT_WINDOWS
            ImGui::End();
#endif
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("双相机扫描"))
        {
                static char scan_nb[64] = "1";
                static char text_hint[64] = "请输入扫描数据长度!";
                // 通过添加文本框的内边距，来实现控制ImVec2(左右, 上下);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
                // 设置圆角
                //ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                // 设置输入框的宽度为 200 像素
                ImGui::PushItemWidth(200);
                ImGui::Text("扫描长度(ms): ");
                ImGui::SameLine();
                ImGui::InputTextWithHint(" ", text_hint, scan_nb, IM_ARRAYSIZE(scan_nb));
                //ImGui::InputText(" ", scan_nb, IM_ARRAYSIZE(scan_nb), ImGuiInputTextFlags_None, 0, scan_nb);
                // 恢复默认宽度
                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
                /*LOG(INFO) << "scan_nb: " << scan_nb << "\n";
                LOG(INFO) << "scan_nb: " << (scan_nb[0] == '\0') << "\n";*/
                //默认是1
                int scan_times = 1;
                if (scan_nb[0] != '\0')
                    scan_times = std::stoi(scan_nb);

                ImGui::SameLine();
                bool scan_button_pressed = ImGui::SmallButton("开始扫描");
                if (scan_button_pressed) {//, ImVec2(100.0f, 40.0f)
                    ImGui::Text(scan_nb);

                    /*std::thread scanner_config([&]() -> int {
                        rc = scanner_.Config_Scanner(&log_sink);
                        LOG(INFO) << "rc: " << rc;
                        LOG(INFO) << "scanner config thread done ...!";
                        return rc;
                        });
                    scanner_config.join();*/

                }
            
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("三相机扫描"))
        {
            if (ImGui::SmallButton("开始扫描")) {
                ImGui::Text("开始按相机扫描");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void MainWindow::ShowPageView2()
{
    ImGui::Text("0"	       "        ICON_FA_0                                      ");
    ImGui::Text("1"	       "        ICON_FA_1                                      ");
    ImGui::Text("2"	       "        ICON_FA_2                                      ");
    ImGui::Text("3"	       "        ICON_FA_3                                      ");
    ImGui::Text("4"	       "        ICON_FA_4                                      ");
    ImGui::Text("5"	       "        ICON_FA_5                                      ");
    ImGui::Text("6"	       "        ICON_FA_6                                      ");
    ImGui::Text("7"	       "        ICON_FA_7                                      ");
    ImGui::Text("8"	       "        ICON_FA_8                                      ");
    ImGui::Text("9"	       "        ICON_FA_9                                      ");
    ImGui::Text("A"	       "        ICON_FA_A                                      ");
    ImGui::Text("\xef\x8a\xb9""        ICON_FA_ADDRESS_BOOK                           ");
    ImGui::Text("\xef\x8a\xbb""        ICON_FA_ADDRESS_CARD                           ");
    ImGui::Text("\xef\x80\xb7""        ICON_FA_ALIGN_CENTER                           ");
    ImGui::Text("\xef\x80\xb9""        ICON_FA_ALIGN_JUSTIFY                          ");
    ImGui::Text("\xef\x80\xb6""        ICON_FA_ALIGN_LEFT                             ");
    ImGui::Text("\xef\x80\xb8""        ICON_FA_ALIGN_RIGHT                            ");
    ImGui::Text("\xef\x84\xbd""        ICON_FA_ANCHOR                                 ");
    ImGui::Text("\xee\x92\xaa""        ICON_FA_ANCHOR_CIRCLE_CHECK                    ");
    ImGui::Text("\xee\x92\xab""        ICON_FA_ANCHOR_CIRCLE_EXCLAMATION              ");
    ImGui::Text("\xee\x92\xac""        ICON_FA_ANCHOR_CIRCLE_XMARK                    ");
    ImGui::Text("\xee\x92\xad""        ICON_FA_ANCHOR_LOCK                            ");
    ImGui::Text("\xef\x84\x87""        ICON_FA_ANGLE_DOWN                             ");
    ImGui::Text("\xef\x84\x84""        ICON_FA_ANGLE_LEFT                             ");
    ImGui::Text("\xef\x84\x85""        ICON_FA_ANGLE_RIGHT                            ");
    ImGui::Text("\xef\x84\x86""        ICON_FA_ANGLE_UP                               ");
    ImGui::Text("\xef\x84\x83""        ICON_FA_ANGLES_DOWN                            ");
    ImGui::Text("\xef\x84\x80""        ICON_FA_ANGLES_LEFT                            ");
    ImGui::Text("\xef\x84\x81""        ICON_FA_ANGLES_RIGHT                           ");
    ImGui::Text("\xef\x84\x82""        ICON_FA_ANGLES_UP                              ");
    ImGui::Text("\xef\x99\x84""        ICON_FA_ANKH                                   ");
    ImGui::Text("\xef\x97\x91""        ICON_FA_APPLE_WHOLE                            ");
    ImGui::Text("\xef\x95\x97""        ICON_FA_ARCHWAY                                ");
    ImGui::Text("\xef\x81\xa3""        ICON_FA_ARROW_DOWN                             ");
    ImGui::Text("\xef\x85\xa2""        ICON_FA_ARROW_DOWN_1_9                         ");
    ImGui::Text("\xef\xa2\x86""        ICON_FA_ARROW_DOWN_9_1                         ");
    ImGui::Text("\xef\x85\x9d""        ICON_FA_ARROW_DOWN_A_Z                         ");
    ImGui::Text("\xef\x85\xb5""        ICON_FA_ARROW_DOWN_LONG                        ");
    ImGui::Text("\xef\xa2\x84""        ICON_FA_ARROW_DOWN_SHORT_WIDE                  ");
    ImGui::Text("\xee\x92\xaf""        ICON_FA_ARROW_DOWN_UP_ACROSS_LINE              ");
    ImGui::Text("\xee\x92\xb0""        ICON_FA_ARROW_DOWN_UP_LOCK                     ");
    ImGui::Text("\xef\x85\xa0""        ICON_FA_ARROW_DOWN_WIDE_SHORT                  ");
    ImGui::Text("\xef\xa2\x81""        ICON_FA_ARROW_DOWN_Z_A                         ");
    ImGui::Text("\xef\x81\xa0""        ICON_FA_ARROW_LEFT                             ");
    ImGui::Text("\xef\x85\xb7""        ICON_FA_ARROW_LEFT_LONG                        ");
    ImGui::Text("\xef\x89\x85""        ICON_FA_ARROW_POINTER                          ");
    ImGui::Text("\xef\x81\xa1""        ICON_FA_ARROW_RIGHT                            ");
    ImGui::Text("\xef\x83\xac""        ICON_FA_ARROW_RIGHT_ARROW_LEFT                 ");
    ImGui::Text("\xef\x82\x8b""        ICON_FA_ARROW_RIGHT_FROM_BRACKET               ");
    ImGui::Text("\xef\x85\xb8""        ICON_FA_ARROW_RIGHT_LONG                       ");
    ImGui::Text("\xef\x82\x90""        ICON_FA_ARROW_RIGHT_TO_BRACKET                 ");
    ImGui::Text("\xee\x92\xb3""        ICON_FA_ARROW_RIGHT_TO_CITY                    ");
    ImGui::Text("\xef\x83\xa2""        ICON_FA_ARROW_ROTATE_LEFT                      ");
    ImGui::Text("\xef\x80\x9e""        ICON_FA_ARROW_ROTATE_RIGHT                     ");
    ImGui::Text("\xee\x82\x97""        ICON_FA_ARROW_TREND_DOWN                       ");
    ImGui::Text("\xee\x82\x98""        ICON_FA_ARROW_TREND_UP                         ");
    ImGui::Text("\xef\x85\x89""        ICON_FA_ARROW_TURN_DOWN                        ");
    ImGui::Text("\xef\x85\x88""        ICON_FA_ARROW_TURN_UP                          ");
    ImGui::Text("\xef\x81\xa2""        ICON_FA_ARROW_UP                               ");
    ImGui::Text("\xef\x85\xa3""        ICON_FA_ARROW_UP_1_9                           ");
    ImGui::Text("\xef\xa2\x87""        ICON_FA_ARROW_UP_9_1                           ");
    ImGui::Text("\xef\x85\x9e""        ICON_FA_ARROW_UP_A_Z                           ");
    ImGui::Text("\xee\x82\x9a""        ICON_FA_ARROW_UP_FROM_BRACKET                  ");
    ImGui::Text("\xee\x92\xb5""        ICON_FA_ARROW_UP_FROM_GROUND_WATER             ");
    ImGui::Text("\xee\x92\xb6""        ICON_FA_ARROW_UP_FROM_WATER_PUMP               ");
    ImGui::Text("\xef\x85\xb6""        ICON_FA_ARROW_UP_LONG                          ");
    ImGui::Text("\xee\x92\xb7""        ICON_FA_ARROW_UP_RIGHT_DOTS                    ");
    ImGui::Text("\xef\x82\x8e""        ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE             ");
    ImGui::Text("\xef\xa2\x85""        ICON_FA_ARROW_UP_SHORT_WIDE                    ");
    ImGui::Text("\xef\x85\xa1""        ICON_FA_ARROW_UP_WIDE_SHORT                    ");
    ImGui::Text("\xef\xa2\x82""        ICON_FA_ARROW_UP_Z_A                           ");
    ImGui::Text("\xee\x92\xb8""        ICON_FA_ARROWS_DOWN_TO_LINE                    ");
    ImGui::Text("\xee\x92\xb9""        ICON_FA_ARROWS_DOWN_TO_PEOPLE                  ");
    ImGui::Text("\xef\x81\xbe""        ICON_FA_ARROWS_LEFT_RIGHT                      ");
    ImGui::Text("\xee\x92\xba""        ICON_FA_ARROWS_LEFT_RIGHT_TO_LINE              ");
    ImGui::Text("\xef\x80\xa1""        ICON_FA_ARROWS_ROTATE                          ");
    ImGui::Text("\xee\x92\xbb""        ICON_FA_ARROWS_SPIN                            ");
    ImGui::Text("\xee\x92\xbc""        ICON_FA_ARROWS_SPLIT_UP_AND_LEFT               ");
    ImGui::Text("\xee\x92\xbd""        ICON_FA_ARROWS_TO_CIRCLE                       ");
    ImGui::Text("\xee\x92\xbe""        ICON_FA_ARROWS_TO_DOT                          ");
    ImGui::Text("\xee\x92\xbf""        ICON_FA_ARROWS_TO_EYE                          ");
    ImGui::Text("\xee\x93\x80""        ICON_FA_ARROWS_TURN_RIGHT                      ");
    ImGui::Text("\xee\x93\x81""        ICON_FA_ARROWS_TURN_TO_DOTS                    ");
    ImGui::Text("\xef\x81\xbd""        ICON_FA_ARROWS_UP_DOWN                         ");
    ImGui::Text("\xef\x81\x87""        ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT              ");
    ImGui::Text("\xee\x93\x82""        ICON_FA_ARROWS_UP_TO_LINE                      ");
    ImGui::Text("*"	       "        ICON_FA_ASTERISK                               ");
    ImGui::Text("@"	       "        ICON_FA_AT                                     ");
    ImGui::Text("\xef\x97\x92""        ICON_FA_ATOM                                   ");
    ImGui::Text("\xef\x8a\x9e""        ICON_FA_AUDIO_DESCRIPTION                      ");
    ImGui::Text("\xee\x82\xa9""        ICON_FA_AUSTRAL_SIGN                           ");
    ImGui::Text("\xef\x95\x99""        ICON_FA_AWARD                                  ");
    ImGui::Text("B"	       "        ICON_FA_B                                      ");
    ImGui::Text("\xef\x9d\xbc""        ICON_FA_BABY                                   ");
    ImGui::Text("\xef\x9d\xbd""        ICON_FA_BABY_CARRIAGE                          ");
    ImGui::Text("\xef\x81\x8a""        ICON_FA_BACKWARD                               ");
    ImGui::Text("\xef\x81\x89""        ICON_FA_BACKWARD_FAST                          ");
    ImGui::Text("\xef\x81\x88""        ICON_FA_BACKWARD_STEP                          ");
    ImGui::Text("\xef\x9f\xa5""        ICON_FA_BACON                                  ");
    ImGui::Text("\xee\x81\x99""        ICON_FA_BACTERIA                               ");
    ImGui::Text("\xee\x81\x9a""        ICON_FA_BACTERIUM                              ");
    ImGui::Text("\xef\x8a\x90""        ICON_FA_BAG_SHOPPING                           ");
    ImGui::Text("\xef\x99\xa6""        ICON_FA_BAHAI                                  ");
    ImGui::Text("\xee\x82\xac""        ICON_FA_BAHT_SIGN                              ");
    ImGui::Text("\xef\x81\x9e""        ICON_FA_BAN                                    ");
    ImGui::Text("\xef\x95\x8d""        ICON_FA_BAN_SMOKING                            ");
    ImGui::Text("\xef\x91\xa2""        ICON_FA_BANDAGE                                ");
    ImGui::Text("\xee\x8b\xa6""        ICON_FA_BANGLADESHI_TAKA_SIGN                  ");
    ImGui::Text("\xef\x80\xaa""        ICON_FA_BARCODE                                ");
    ImGui::Text("\xef\x83\x89""        ICON_FA_BARS                                   ");
    ImGui::Text("\xef\xa0\xa8""        ICON_FA_BARS_PROGRESS                          ");
    ImGui::Text("\xef\x95\x90""        ICON_FA_BARS_STAGGERED                         ");
    ImGui::Text("\xef\x90\xb3""        ICON_FA_BASEBALL                               ");
    ImGui::Text("\xef\x90\xb2""        ICON_FA_BASEBALL_BAT_BALL                      ");
    ImGui::Text("\xef\x8a\x91""        ICON_FA_BASKET_SHOPPING                        ");
    ImGui::Text("\xef\x90\xb4""        ICON_FA_BASKETBALL                             ");
    ImGui::Text("\xef\x8b\x8d""        ICON_FA_BATH                                   ");
    ImGui::Text("\xef\x89\x84""        ICON_FA_BATTERY_EMPTY                          ");
    ImGui::Text("\xef\x89\x80""        ICON_FA_BATTERY_FULL                           ");
    ImGui::Text("\xef\x89\x82""        ICON_FA_BATTERY_HALF                           ");
    ImGui::Text("\xef\x89\x83""        ICON_FA_BATTERY_QUARTER                        ");
    ImGui::Text("\xef\x89\x81""        ICON_FA_BATTERY_THREE_QUARTERS                 ");
    ImGui::Text("\xef\x88\xb6""        ICON_FA_BED                                    ");
    ImGui::Text("\xef\x92\x87""        ICON_FA_BED_PULSE                              ");
    ImGui::Text("\xef\x83\xbc""        ICON_FA_BEER_MUG_EMPTY                         ");
    ImGui::Text("\xef\x83\xb3""        ICON_FA_BELL                                   ");
    ImGui::Text("\xef\x95\xa2""        ICON_FA_BELL_CONCIERGE                         ");
    ImGui::Text("\xef\x87\xb6""        ICON_FA_BELL_SLASH                             ");
    ImGui::Text("\xef\x95\x9b""        ICON_FA_BEZIER_CURVE                           ");
    ImGui::Text("\xef\x88\x86""        ICON_FA_BICYCLE                                ");
    ImGui::Text("\xef\x87\xa5""        ICON_FA_BINOCULARS                             ");
    ImGui::Text("\xef\x9e\x80""        ICON_FA_BIOHAZARD                              ");
    ImGui::Text("\xee\x82\xb4""        ICON_FA_BITCOIN_SIGN                           ");
    ImGui::Text("\xef\x94\x97""        ICON_FA_BLENDER                                ");
    ImGui::Text("\xef\x9a\xb6""        ICON_FA_BLENDER_PHONE                          ");
    ImGui::Text("\xef\x9e\x81""        ICON_FA_BLOG                                   ");
    ImGui::Text("\xef\x80\xb2""        ICON_FA_BOLD                                   ");
    ImGui::Text("\xef\x83\xa7""        ICON_FA_BOLT                                   ");
    ImGui::Text("\xee\x82\xb7""        ICON_FA_BOLT_LIGHTNING                         ");
    ImGui::Text("\xef\x87\xa2""        ICON_FA_BOMB                                   ");
    ImGui::Text("\xef\x97\x97""        ICON_FA_BONE                                   ");
    ImGui::Text("\xef\x95\x9c""        ICON_FA_BONG                                   ");
    ImGui::Text("\xef\x80\xad""        ICON_FA_BOOK                                   ");
    ImGui::Text("\xef\x95\x98""        ICON_FA_BOOK_ATLAS                             ");
    ImGui::Text("\xef\x99\x87""        ICON_FA_BOOK_BIBLE                             ");
    ImGui::Text("\xee\x82\xbb""        ICON_FA_BOOK_BOOKMARK                          ");
    ImGui::Text("\xef\x99\xaa""        ICON_FA_BOOK_JOURNAL_WHILLS                    ");
    ImGui::Text("\xef\x9f\xa6""        ICON_FA_BOOK_MEDICAL                           ");
    ImGui::Text("\xef\x94\x98""        ICON_FA_BOOK_OPEN                              ");
    ImGui::Text("\xef\x97\x9a""        ICON_FA_BOOK_OPEN_READER                       ");
    ImGui::Text("\xef\x9a\x87""        ICON_FA_BOOK_QURAN                             ");
    ImGui::Text("\xef\x9a\xb7""        ICON_FA_BOOK_SKULL                             ");
    ImGui::Text("\xef\xa0\xa7""        ICON_FA_BOOK_TANAKH                            ");
    ImGui::Text("\xef\x80\xae""        ICON_FA_BOOKMARK                               ");
    ImGui::Text("\xef\xa1\x8c""        ICON_FA_BORDER_ALL                             ");
    ImGui::Text("\xef\xa1\x90""        ICON_FA_BORDER_NONE                            ");
    ImGui::Text("\xef\xa1\x93""        ICON_FA_BORDER_TOP_LEFT                        ");
    ImGui::Text("\xee\x93\x83""        ICON_FA_BORE_HOLE                              ");
    ImGui::Text("\xee\x93\x84""        ICON_FA_BOTTLE_DROPLET                         ");
    ImGui::Text("\xee\x93\x85""        ICON_FA_BOTTLE_WATER                           ");
    ImGui::Text("\xee\x93\x86""        ICON_FA_BOWL_FOOD                              ");
    ImGui::Text("\xee\x8b\xab""        ICON_FA_BOWL_RICE                              ");
    ImGui::Text("\xef\x90\xb6""        ICON_FA_BOWLING_BALL                           ");
    ImGui::Text("\xef\x91\xa6""        ICON_FA_BOX                                    ");
    ImGui::Text("\xef\x86\x87""        ICON_FA_BOX_ARCHIVE                            ");
    ImGui::Text("\xef\x92\x9e""        ICON_FA_BOX_OPEN                               ");
    ImGui::Text("\xee\x81\x9b""        ICON_FA_BOX_TISSUE                             ");
    ImGui::Text("\xee\x93\x87""        ICON_FA_BOXES_PACKING                          ");
    ImGui::Text("\xef\x91\xa8""        ICON_FA_BOXES_STACKED                          ");
    ImGui::Text("\xef\x8a\xa1""        ICON_FA_BRAILLE                                ");
    ImGui::Text("\xef\x97\x9c""        ICON_FA_BRAIN                                  ");
    ImGui::Text("\xee\x91\xac""        ICON_FA_BRAZILIAN_REAL_SIGN                    ");
    ImGui::Text("\xef\x9f\xac""        ICON_FA_BREAD_SLICE                            ");
    ImGui::Text("\xee\x93\x88""        ICON_FA_BRIDGE                                 ");
    ImGui::Text("\xee\x93\x89""        ICON_FA_BRIDGE_CIRCLE_CHECK                    ");
    ImGui::Text("\xee\x93\x8a""        ICON_FA_BRIDGE_CIRCLE_EXCLAMATION              ");
    ImGui::Text("\xee\x93\x8b""        ICON_FA_BRIDGE_CIRCLE_XMARK                    ");
    ImGui::Text("\xee\x93\x8c""        ICON_FA_BRIDGE_LOCK                            ");
    ImGui::Text("\xee\x93\x8e""        ICON_FA_BRIDGE_WATER                           ");
    ImGui::Text("\xef\x82\xb1""        ICON_FA_BRIEFCASE                              ");
    ImGui::Text("\xef\x91\xa9""        ICON_FA_BRIEFCASE_MEDICAL                      ");
    ImGui::Text("\xef\x94\x9a""        ICON_FA_BROOM                                  ");
    ImGui::Text("\xef\x91\x98""        ICON_FA_BROOM_BALL                             ");
    ImGui::Text("\xef\x95\x9d""        ICON_FA_BRUSH                                  ");
    ImGui::Text("\xee\x93\x8f""        ICON_FA_BUCKET                                 ");
    ImGui::Text("\xef\x86\x88""        ICON_FA_BUG                                    ");
    ImGui::Text("\xee\x92\x90""        ICON_FA_BUG_SLASH                              ");
    ImGui::Text("\xee\x93\x90""        ICON_FA_BUGS                                   ");
    ImGui::Text("\xef\x86\xad""        ICON_FA_BUILDING                               ");
    ImGui::Text("\xee\x93\x91""        ICON_FA_BUILDING_CIRCLE_ARROW_RIGHT            ");
    ImGui::Text("\xee\x93\x92""        ICON_FA_BUILDING_CIRCLE_CHECK                  ");
    ImGui::Text("\xee\x93\x93""        ICON_FA_BUILDING_CIRCLE_EXCLAMATION            ");
    ImGui::Text("\xee\x93\x94""        ICON_FA_BUILDING_CIRCLE_XMARK                  ");
    ImGui::Text("\xef\x86\x9c""        ICON_FA_BUILDING_COLUMNS                       ");
    ImGui::Text("\xee\x93\x95""        ICON_FA_BUILDING_FLAG                          ");
    ImGui::Text("\xee\x93\x96""        ICON_FA_BUILDING_LOCK                          ");
    ImGui::Text("\xee\x93\x97""        ICON_FA_BUILDING_NGO                           ");
    ImGui::Text("\xee\x93\x98""        ICON_FA_BUILDING_SHIELD                        ");
    ImGui::Text("\xee\x93\x99""        ICON_FA_BUILDING_UN                            ");
    ImGui::Text("\xee\x93\x9a""        ICON_FA_BUILDING_USER                          ");
    ImGui::Text("\xee\x93\x9b""        ICON_FA_BUILDING_WHEAT                         ");
    ImGui::Text("\xef\x82\xa1""        ICON_FA_BULLHORN                               ");
    ImGui::Text("\xef\x85\x80""        ICON_FA_BULLSEYE                               ");
    ImGui::Text("\xef\xa0\x85""        ICON_FA_BURGER                                 ");
    ImGui::Text("\xee\x93\x9c""        ICON_FA_BURST                                  ");
    ImGui::Text("\xef\x88\x87""        ICON_FA_BUS                                    ");
    ImGui::Text("\xef\x95\x9e""        ICON_FA_BUS_SIMPLE                             ");
    ImGui::Text("\xef\x99\x8a""        ICON_FA_BUSINESS_TIME                          ");
    ImGui::Text("C"	       "        ICON_FA_C                                      ");
    ImGui::Text("\xef\x9f\x9a""        ICON_FA_CABLE_CAR                              ");
    ImGui::Text("\xef\x87\xbd""        ICON_FA_CAKE_CANDLES                           ");
    ImGui::Text("\xef\x87\xac""        ICON_FA_CALCULATOR                             ");
    ImGui::Text("\xef\x84\xb3""        ICON_FA_CALENDAR                               ");
    ImGui::Text("\xef\x89\xb4""        ICON_FA_CALENDAR_CHECK                         ");
    ImGui::Text("\xef\x9e\x83""        ICON_FA_CALENDAR_DAY                           ");
    ImGui::Text("\xef\x81\xb3""        ICON_FA_CALENDAR_DAYS                          ");
    ImGui::Text("\xef\x89\xb2""        ICON_FA_CALENDAR_MINUS                         ");
    ImGui::Text("\xef\x89\xb1""        ICON_FA_CALENDAR_PLUS                          ");
    ImGui::Text("\xef\x9e\x84""        ICON_FA_CALENDAR_WEEK                          ");
    ImGui::Text("\xef\x89\xb3""        ICON_FA_CALENDAR_XMARK                         ");
    ImGui::Text("\xef\x80\xb0""        ICON_FA_CAMERA                                 ");
    ImGui::Text("\xef\x82\x83""        ICON_FA_CAMERA_RETRO                           ");
    ImGui::Text("\xee\x83\x98""        ICON_FA_CAMERA_ROTATE                          ");
    ImGui::Text("\xef\x9a\xbb""        ICON_FA_CAMPGROUND                             ");
    ImGui::Text("\xef\x9e\x86""        ICON_FA_CANDY_CANE                             ");
    ImGui::Text("\xef\x95\x9f""        ICON_FA_CANNABIS                               ");
    ImGui::Text("\xef\x91\xab""        ICON_FA_CAPSULES                               ");
    ImGui::Text("\xef\x86\xb9""        ICON_FA_CAR                                    ");
    ImGui::Text("\xef\x97\x9f""        ICON_FA_CAR_BATTERY                            ");
    ImGui::Text("\xef\x97\xa1""        ICON_FA_CAR_BURST                              ");
    ImGui::Text("\xee\x93\x9d""        ICON_FA_CAR_ON                                 ");
    ImGui::Text("\xef\x97\x9e""        ICON_FA_CAR_REAR                               ");
    ImGui::Text("\xef\x97\xa4""        ICON_FA_CAR_SIDE                               ");
    ImGui::Text("\xee\x93\x9e""        ICON_FA_CAR_TUNNEL                             ");
    ImGui::Text("\xef\xa3\xbf""        ICON_FA_CARAVAN                                ");
    ImGui::Text("\xef\x83\x97""        ICON_FA_CARET_DOWN                             ");
    ImGui::Text("\xef\x83\x99""        ICON_FA_CARET_LEFT                             ");
    ImGui::Text("\xef\x83\x9a""        ICON_FA_CARET_RIGHT                            ");
    ImGui::Text("\xef\x83\x98""        ICON_FA_CARET_UP                               ");
    ImGui::Text("\xef\x9e\x87""        ICON_FA_CARROT                                 ");
    ImGui::Text("\xef\x88\x98""        ICON_FA_CART_ARROW_DOWN                        ");
    ImGui::Text("\xef\x91\xb4""        ICON_FA_CART_FLATBED                           ");
    ImGui::Text("\xef\x96\x9d""        ICON_FA_CART_FLATBED_SUITCASE                  ");
    ImGui::Text("\xef\x88\x97""        ICON_FA_CART_PLUS                              ");
    ImGui::Text("\xef\x81\xba""        ICON_FA_CART_SHOPPING                          ");
    ImGui::Text("\xef\x9e\x88""        ICON_FA_CASH_REGISTER                          ");
    ImGui::Text("\xef\x9a\xbe""        ICON_FA_CAT                                    ");
    ImGui::Text("\xee\x83\x9f""        ICON_FA_CEDI_SIGN                              ");
    ImGui::Text("\xee\x8f\xb5""        ICON_FA_CENT_SIGN                              ");
    ImGui::Text("\xef\x82\xa3""        ICON_FA_CERTIFICATE                            ");
    ImGui::Text("\xef\x9b\x80""        ICON_FA_CHAIR                                  ");
    ImGui::Text("\xef\x94\x9b""        ICON_FA_CHALKBOARD                             ");
    ImGui::Text("\xef\x94\x9c""        ICON_FA_CHALKBOARD_USER                        ");
    ImGui::Text("\xef\x9e\x9f""        ICON_FA_CHAMPAGNE_GLASSES                      ");
    ImGui::Text("\xef\x97\xa7""        ICON_FA_CHARGING_STATION                       ");
    ImGui::Text("\xef\x87\xbe""        ICON_FA_CHART_AREA                             ");
    ImGui::Text("\xef\x82\x80""        ICON_FA_CHART_BAR                              ");
    ImGui::Text("\xee\x83\xa3""        ICON_FA_CHART_COLUMN                           ");
    ImGui::Text("\xee\x83\xa4""        ICON_FA_CHART_GANTT                            ");
    ImGui::Text("\xef\x88\x81""        ICON_FA_CHART_LINE                             ");
    ImGui::Text("\xef\x88\x80""        ICON_FA_CHART_PIE                              ");
    ImGui::Text("\xee\x91\xb3""        ICON_FA_CHART_SIMPLE                           ");
    ImGui::Text("\xef\x80\x8c""        ICON_FA_CHECK                                  ");
    ImGui::Text("\xef\x95\xa0""        ICON_FA_CHECK_DOUBLE                           ");
    ImGui::Text("\xef\x9d\xb2""        ICON_FA_CHECK_TO_SLOT                          ");
    ImGui::Text("\xef\x9f\xaf""        ICON_FA_CHEESE                                 ");
    ImGui::Text("\xef\x90\xb9""        ICON_FA_CHESS                                  ");
    ImGui::Text("\xef\x90\xba""        ICON_FA_CHESS_BISHOP                           ");
    ImGui::Text("\xef\x90\xbc""        ICON_FA_CHESS_BOARD                            ");
    ImGui::Text("\xef\x90\xbf""        ICON_FA_CHESS_KING                             ");
    ImGui::Text("\xef\x91\x81""        ICON_FA_CHESS_KNIGHT                           ");
    ImGui::Text("\xef\x91\x83""        ICON_FA_CHESS_PAWN                             ");
    ImGui::Text("\xef\x91\x85""        ICON_FA_CHESS_QUEEN                            ");
    ImGui::Text("\xef\x91\x87""        ICON_FA_CHESS_ROOK                             ");
    ImGui::Text("\xef\x81\xb8""        ICON_FA_CHEVRON_DOWN                           ");
    ImGui::Text("\xef\x81\x93""        ICON_FA_CHEVRON_LEFT                           ");
    ImGui::Text("\xef\x81\x94""        ICON_FA_CHEVRON_RIGHT                          ");
    ImGui::Text("\xef\x81\xb7""        ICON_FA_CHEVRON_UP                             ");
    ImGui::Text("\xef\x86\xae""        ICON_FA_CHILD                                  ");
    ImGui::Text("\xee\x93\xa0""        ICON_FA_CHILD_COMBATANT                        ");
    ImGui::Text("\xee\x96\x9c""        ICON_FA_CHILD_DRESS                            ");
    ImGui::Text("\xee\x96\x9d""        ICON_FA_CHILD_REACHING                         ");
    ImGui::Text("\xee\x93\xa1""        ICON_FA_CHILDREN                               ");
    ImGui::Text("\xef\x94\x9d""        ICON_FA_CHURCH                                 ");
    ImGui::Text("\xef\x84\x91""        ICON_FA_CIRCLE                                 ");
    ImGui::Text("\xef\x82\xab""        ICON_FA_CIRCLE_ARROW_DOWN                      ");
    ImGui::Text("\xef\x82\xa8""        ICON_FA_CIRCLE_ARROW_LEFT                      ");
    ImGui::Text("\xef\x82\xa9""        ICON_FA_CIRCLE_ARROW_RIGHT                     ");
    ImGui::Text("\xef\x82\xaa""        ICON_FA_CIRCLE_ARROW_UP                        ");
    ImGui::Text("\xef\x81\x98""        ICON_FA_CIRCLE_CHECK                           ");
    ImGui::Text("\xef\x84\xba""        ICON_FA_CIRCLE_CHEVRON_DOWN                    ");
    ImGui::Text("\xef\x84\xb7""        ICON_FA_CIRCLE_CHEVRON_LEFT                    ");
    ImGui::Text("\xef\x84\xb8""        ICON_FA_CIRCLE_CHEVRON_RIGHT                   ");
    ImGui::Text("\xef\x84\xb9""        ICON_FA_CIRCLE_CHEVRON_UP                      ");
    ImGui::Text("\xef\x92\xb9""        ICON_FA_CIRCLE_DOLLAR_TO_SLOT                  ");
    ImGui::Text("\xef\x86\x92""        ICON_FA_CIRCLE_DOT                             ");
    ImGui::Text("\xef\x8d\x98""        ICON_FA_CIRCLE_DOWN                            ");
    ImGui::Text("\xef\x81\xaa""        ICON_FA_CIRCLE_EXCLAMATION                     ");
    ImGui::Text("\xef\x91\xbe""        ICON_FA_CIRCLE_H                               ");
    ImGui::Text("\xef\x81\x82""        ICON_FA_CIRCLE_HALF_STROKE                     ");
    ImGui::Text("\xef\x81\x9a""        ICON_FA_CIRCLE_INFO                            ");
    ImGui::Text("\xef\x8d\x99""        ICON_FA_CIRCLE_LEFT                            ");
    ImGui::Text("\xef\x81\x96""        ICON_FA_CIRCLE_MINUS                           ");
    ImGui::Text("\xee\x93\xa2""        ICON_FA_CIRCLE_NODES                           ");
    ImGui::Text("\xef\x87\x8e""        ICON_FA_CIRCLE_NOTCH                           ");
    ImGui::Text("\xef\x8a\x8b""        ICON_FA_CIRCLE_PAUSE                           ");
    ImGui::Text("\xef\x85\x84""        ICON_FA_CIRCLE_PLAY                            ");
    ImGui::Text("\xef\x81\x95""        ICON_FA_CIRCLE_PLUS                            ");
    ImGui::Text("\xef\x81\x99""        ICON_FA_CIRCLE_QUESTION                        ");
    ImGui::Text("\xef\x9e\xba""        ICON_FA_CIRCLE_RADIATION                       ");
    ImGui::Text("\xef\x8d\x9a""        ICON_FA_CIRCLE_RIGHT                           ");
    ImGui::Text("\xef\x8a\x8d""        ICON_FA_CIRCLE_STOP                            ");
    ImGui::Text("\xef\x8d\x9b""        ICON_FA_CIRCLE_UP                              ");
    ImGui::Text("\xef\x8a\xbd""        ICON_FA_CIRCLE_USER                            ");
    ImGui::Text("\xef\x81\x97""        ICON_FA_CIRCLE_XMARK                           ");
    ImGui::Text("\xef\x99\x8f""        ICON_FA_CITY                                   ");
    ImGui::Text("\xee\x84\xb1""        ICON_FA_CLAPPERBOARD                           ");
    ImGui::Text("\xef\x8c\xa8""        ICON_FA_CLIPBOARD                              ");
    ImGui::Text("\xef\x91\xac""        ICON_FA_CLIPBOARD_CHECK                        ");
    ImGui::Text("\xef\x91\xad""        ICON_FA_CLIPBOARD_LIST                         ");
    ImGui::Text("\xee\x93\xa3""        ICON_FA_CLIPBOARD_QUESTION                     ");
    ImGui::Text("\xef\x9f\xb3""        ICON_FA_CLIPBOARD_USER                         ");
    ImGui::Text("\xef\x80\x97""        ICON_FA_CLOCK                                  ");
    ImGui::Text("\xef\x87\x9a""        ICON_FA_CLOCK_ROTATE_LEFT                      ");
    ImGui::Text("\xef\x89\x8d""        ICON_FA_CLONE                                  ");
    ImGui::Text("\xef\x88\x8a""        ICON_FA_CLOSED_CAPTIONING                      ");
    ImGui::Text("\xef\x83\x82""        ICON_FA_CLOUD                                  ");
    ImGui::Text("\xef\x83\xad""        ICON_FA_CLOUD_ARROW_DOWN                       ");
    ImGui::Text("\xef\x83\xae""        ICON_FA_CLOUD_ARROW_UP                         ");
    ImGui::Text("\xef\x9d\xac""        ICON_FA_CLOUD_BOLT                             ");
    ImGui::Text("\xef\x9c\xbb""        ICON_FA_CLOUD_MEATBALL                         ");
    ImGui::Text("\xef\x9b\x83""        ICON_FA_CLOUD_MOON                             ");
    ImGui::Text("\xef\x9c\xbc""        ICON_FA_CLOUD_MOON_RAIN                        ");
    ImGui::Text("\xef\x9c\xbd""        ICON_FA_CLOUD_RAIN                             ");
    ImGui::Text("\xef\x9d\x80""        ICON_FA_CLOUD_SHOWERS_HEAVY                    ");
    ImGui::Text("\xee\x93\xa4""        ICON_FA_CLOUD_SHOWERS_WATER                    ");
    ImGui::Text("\xef\x9b\x84""        ICON_FA_CLOUD_SUN                              ");
    ImGui::Text("\xef\x9d\x83""        ICON_FA_CLOUD_SUN_RAIN                         ");
    ImGui::Text("\xee\x84\xb9""        ICON_FA_CLOVER                                 ");
    ImGui::Text("\xef\x84\xa1""        ICON_FA_CODE                                   ");
    ImGui::Text("\xef\x84\xa6""        ICON_FA_CODE_BRANCH                            ");
    ImGui::Text("\xef\x8e\x86""        ICON_FA_CODE_COMMIT                            ");
    ImGui::Text("\xee\x84\xba""        ICON_FA_CODE_COMPARE                           ");
    ImGui::Text("\xee\x84\xbb""        ICON_FA_CODE_FORK                              ");
    ImGui::Text("\xef\x8e\x87""        ICON_FA_CODE_MERGE                             ");
    ImGui::Text("\xee\x84\xbc""        ICON_FA_CODE_PULL_REQUEST                      ");
    ImGui::Text("\xef\x94\x9e""        ICON_FA_COINS                                  ");
    ImGui::Text("\xee\x85\x80""        ICON_FA_COLON_SIGN                             ");
    ImGui::Text("\xef\x81\xb5""        ICON_FA_COMMENT                                ");
    ImGui::Text("\xef\x99\x91""        ICON_FA_COMMENT_DOLLAR                         ");
    ImGui::Text("\xef\x92\xad""        ICON_FA_COMMENT_DOTS                           ");
    ImGui::Text("\xef\x9f\xb5""        ICON_FA_COMMENT_MEDICAL                        ");
    ImGui::Text("\xef\x92\xb3""        ICON_FA_COMMENT_SLASH                          ");
    ImGui::Text("\xef\x9f\x8d""        ICON_FA_COMMENT_SMS                            ");
    ImGui::Text("\xef\x82\x86""        ICON_FA_COMMENTS                               ");
    ImGui::Text("\xef\x99\x93""        ICON_FA_COMMENTS_DOLLAR                        ");
    ImGui::Text("\xef\x94\x9f""        ICON_FA_COMPACT_DISC                           ");
    ImGui::Text("\xef\x85\x8e""        ICON_FA_COMPASS                                ");
    ImGui::Text("\xef\x95\xa8""        ICON_FA_COMPASS_DRAFTING                       ");
    ImGui::Text("\xef\x81\xa6""        ICON_FA_COMPRESS                               ");
    ImGui::Text("\xee\x93\xa5""        ICON_FA_COMPUTER                               ");
    ImGui::Text("\xef\xa3\x8c""        ICON_FA_COMPUTER_MOUSE                         ");
    ImGui::Text("\xef\x95\xa3""        ICON_FA_COOKIE                                 ");
    ImGui::Text("\xef\x95\xa4""        ICON_FA_COOKIE_BITE                            ");
    ImGui::Text("\xef\x83\x85""        ICON_FA_COPY                                   ");
    ImGui::Text("\xef\x87\xb9""        ICON_FA_COPYRIGHT                              ");
    ImGui::Text("\xef\x92\xb8""        ICON_FA_COUCH                                  ");
    ImGui::Text("\xef\x9b\x88""        ICON_FA_COW                                    ");
    ImGui::Text("\xef\x82\x9d""        ICON_FA_CREDIT_CARD                            ");
    ImGui::Text("\xef\x84\xa5""        ICON_FA_CROP                                   ");
    ImGui::Text("\xef\x95\xa5""        ICON_FA_CROP_SIMPLE                            ");
    ImGui::Text("\xef\x99\x94""        ICON_FA_CROSS                                  ");
    ImGui::Text("\xef\x81\x9b""        ICON_FA_CROSSHAIRS                             ");
    ImGui::Text("\xef\x94\xa0""        ICON_FA_CROW                                   ");
    ImGui::Text("\xef\x94\xa1""        ICON_FA_CROWN                                  ");
    ImGui::Text("\xef\x9f\xb7""        ICON_FA_CRUTCH                                 ");
    ImGui::Text("\xee\x85\x92""        ICON_FA_CRUZEIRO_SIGN                          ");
    ImGui::Text("\xef\x86\xb2""        ICON_FA_CUBE                                   ");
    ImGui::Text("\xef\x86\xb3""        ICON_FA_CUBES                                  ");
    ImGui::Text("\xee\x93\xa6""        ICON_FA_CUBES_STACKED                          ");
    ImGui::Text("D"	       "        ICON_FA_D                                      ");
    ImGui::Text("\xef\x87\x80""        ICON_FA_DATABASE                               ");
    ImGui::Text("\xef\x95\x9a""        ICON_FA_DELETE_LEFT                            ");
    ImGui::Text("\xef\x9d\x87""        ICON_FA_DEMOCRAT                               ");
    ImGui::Text("\xef\x8e\x90""        ICON_FA_DESKTOP                                ");
    ImGui::Text("\xef\x99\x95""        ICON_FA_DHARMACHAKRA                           ");
    ImGui::Text("\xee\x91\xb6""        ICON_FA_DIAGRAM_NEXT                           ");
    ImGui::Text("\xee\x91\xb7""        ICON_FA_DIAGRAM_PREDECESSOR                    ");
    ImGui::Text("\xef\x95\x82""        ICON_FA_DIAGRAM_PROJECT                        ");
    ImGui::Text("\xee\x91\xba""        ICON_FA_DIAGRAM_SUCCESSOR                      ");
    ImGui::Text("\xef\x88\x99""        ICON_FA_DIAMOND                                ");
    ImGui::Text("\xef\x97\xab""        ICON_FA_DIAMOND_TURN_RIGHT                     ");
    ImGui::Text("\xef\x94\xa2""        ICON_FA_DICE                                   ");
    ImGui::Text("\xef\x9b\x8f""        ICON_FA_DICE_D20                               ");
    ImGui::Text("\xef\x9b\x91""        ICON_FA_DICE_D6                                ");
    ImGui::Text("\xef\x94\xa3""        ICON_FA_DICE_FIVE                              ");
    ImGui::Text("\xef\x94\xa4""        ICON_FA_DICE_FOUR                              ");
    ImGui::Text("\xef\x94\xa5""        ICON_FA_DICE_ONE                               ");
    ImGui::Text("\xef\x94\xa6""        ICON_FA_DICE_SIX                               ");
    ImGui::Text("\xef\x94\xa7""        ICON_FA_DICE_THREE                             ");
    ImGui::Text("\xef\x94\xa8""        ICON_FA_DICE_TWO                               ");
    ImGui::Text("\xef\x9f\xba""        ICON_FA_DISEASE                                ");
    ImGui::Text("\xee\x85\xa3""        ICON_FA_DISPLAY                                ");
    ImGui::Text("\xef\x94\xa9""        ICON_FA_DIVIDE                                 ");
    ImGui::Text("\xef\x91\xb1""        ICON_FA_DNA                                    ");
    ImGui::Text("\xef\x9b\x93""        ICON_FA_DOG                                    ");
    ImGui::Text("$"	       "        ICON_FA_DOLLAR_SIGN                            ");
    ImGui::Text("\xef\x91\xb2""        ICON_FA_DOLLY                                  ");
    ImGui::Text("\xee\x85\xa9""        ICON_FA_DONG_SIGN                              ");
    ImGui::Text("\xef\x94\xaa""        ICON_FA_DOOR_CLOSED                            ");
    ImGui::Text("\xef\x94\xab""        ICON_FA_DOOR_OPEN                              ");
    ImGui::Text("\xef\x92\xba""        ICON_FA_DOVE                                   ");
    ImGui::Text("\xef\x90\xa2""        ICON_FA_DOWN_LEFT_AND_UP_RIGHT_TO_CENTER       ");
    ImGui::Text("\xef\x8c\x89""        ICON_FA_DOWN_LONG                              ");
    ImGui::Text("\xef\x80\x99""        ICON_FA_DOWNLOAD                               ");
    ImGui::Text("\xef\x9b\x95""        ICON_FA_DRAGON                                 ");
    ImGui::Text("\xef\x97\xae""        ICON_FA_DRAW_POLYGON                           ");
    ImGui::Text("\xef\x81\x83""        ICON_FA_DROPLET                                ");
    ImGui::Text("\xef\x97\x87""        ICON_FA_DROPLET_SLASH                          ");
    ImGui::Text("\xef\x95\xa9""        ICON_FA_DRUM                                   ");
    ImGui::Text("\xef\x95\xaa""        ICON_FA_DRUM_STEELPAN                          ");
    ImGui::Text("\xef\x9b\x97""        ICON_FA_DRUMSTICK_BITE                         ");
    ImGui::Text("\xef\x91\x8b""        ICON_FA_DUMBBELL                               ");
    ImGui::Text("\xef\x9e\x93""        ICON_FA_DUMPSTER                               ");
    ImGui::Text("\xef\x9e\x94""        ICON_FA_DUMPSTER_FIRE                          ");
    ImGui::Text("\xef\x9b\x99""        ICON_FA_DUNGEON                                ");
    ImGui::Text("E"	       "        ICON_FA_E                                      ");
    ImGui::Text("\xef\x8a\xa4""        ICON_FA_EAR_DEAF                               ");
    ImGui::Text("\xef\x8a\xa2""        ICON_FA_EAR_LISTEN                             ");
    ImGui::Text("\xef\x95\xbc""        ICON_FA_EARTH_AFRICA                           ");
    ImGui::Text("\xef\x95\xbd""        ICON_FA_EARTH_AMERICAS                         ");
    ImGui::Text("\xef\x95\xbe""        ICON_FA_EARTH_ASIA                             ");
    ImGui::Text("\xef\x9e\xa2""        ICON_FA_EARTH_EUROPE                           ");
    ImGui::Text("\xee\x91\xbb""        ICON_FA_EARTH_OCEANIA                          ");
    ImGui::Text("\xef\x9f\xbb""        ICON_FA_EGG                                    ");
    ImGui::Text("\xef\x81\x92""        ICON_FA_EJECT                                  ");
    ImGui::Text("\xee\x85\xad""        ICON_FA_ELEVATOR                               ");
    ImGui::Text("\xef\x85\x81""        ICON_FA_ELLIPSIS                               ");
    ImGui::Text("\xef\x85\x82""        ICON_FA_ELLIPSIS_VERTICAL                      ");
    ImGui::Text("\xef\x83\xa0""        ICON_FA_ENVELOPE                               ");
    ImGui::Text("\xee\x93\xa8""        ICON_FA_ENVELOPE_CIRCLE_CHECK                  ");
    ImGui::Text("\xef\x8a\xb6""        ICON_FA_ENVELOPE_OPEN                          ");
    ImGui::Text("\xef\x99\x98""        ICON_FA_ENVELOPE_OPEN_TEXT                     ");
    ImGui::Text("\xef\x99\xb4""        ICON_FA_ENVELOPES_BULK                         ");
    ImGui::Text("="	       "        ICON_FA_EQUALS                                 ");
    ImGui::Text("\xef\x84\xad""        ICON_FA_ERASER                                 ");
    ImGui::Text("\xef\x9e\x96""        ICON_FA_ETHERNET                               ");
    ImGui::Text("\xef\x85\x93""        ICON_FA_EURO_SIGN                              ");
    ImGui::Text("!"	       "        ICON_FA_EXCLAMATION                            ");
    ImGui::Text("\xef\x81\xa5""        ICON_FA_EXPAND                                 ");
    ImGui::Text("\xee\x93\xa9""        ICON_FA_EXPLOSION                              ");
    ImGui::Text("\xef\x81\xae""        ICON_FA_EYE                                    ");
    ImGui::Text("\xef\x87\xbb""        ICON_FA_EYE_DROPPER                            ");
    ImGui::Text("\xef\x8a\xa8""        ICON_FA_EYE_LOW_VISION                         ");
    ImGui::Text("\xef\x81\xb0""        ICON_FA_EYE_SLASH                              ");
    ImGui::Text("F"	       "        ICON_FA_F                                      ");
    ImGui::Text("\xef\x95\x96""        ICON_FA_FACE_ANGRY                             ");
    ImGui::Text("\xef\x95\xa7""        ICON_FA_FACE_DIZZY                             ");
    ImGui::Text("\xef\x95\xb9""        ICON_FA_FACE_FLUSHED                           ");
    ImGui::Text("\xef\x84\x99""        ICON_FA_FACE_FROWN                             ");
    ImGui::Text("\xef\x95\xba""        ICON_FA_FACE_FROWN_OPEN                        ");
    ImGui::Text("\xef\x95\xbf""        ICON_FA_FACE_GRIMACE                           ");
    ImGui::Text("\xef\x96\x80""        ICON_FA_FACE_GRIN                              ");
    ImGui::Text("\xef\x96\x82""        ICON_FA_FACE_GRIN_BEAM                         ");
    ImGui::Text("\xef\x96\x83""        ICON_FA_FACE_GRIN_BEAM_SWEAT                   ");
    ImGui::Text("\xef\x96\x84""        ICON_FA_FACE_GRIN_HEARTS                       ");
    ImGui::Text("\xef\x96\x85""        ICON_FA_FACE_GRIN_SQUINT                       ");
    ImGui::Text("\xef\x96\x86""        ICON_FA_FACE_GRIN_SQUINT_TEARS                 ");
    ImGui::Text("\xef\x96\x87""        ICON_FA_FACE_GRIN_STARS                        ");
    ImGui::Text("\xef\x96\x88""        ICON_FA_FACE_GRIN_TEARS                        ");
    ImGui::Text("\xef\x96\x89""        ICON_FA_FACE_GRIN_TONGUE                       ");
    ImGui::Text("\xef\x96\x8a""        ICON_FA_FACE_GRIN_TONGUE_SQUINT                ");
    ImGui::Text("\xef\x96\x8b""        ICON_FA_FACE_GRIN_TONGUE_WINK                  ");
    ImGui::Text("\xef\x96\x81""        ICON_FA_FACE_GRIN_WIDE                         ");
    ImGui::Text("\xef\x96\x8c""        ICON_FA_FACE_GRIN_WINK                         ");
    ImGui::Text("\xef\x96\x96""        ICON_FA_FACE_KISS                              ");
    ImGui::Text("\xef\x96\x97""        ICON_FA_FACE_KISS_BEAM                         ");
    ImGui::Text("\xef\x96\x98""        ICON_FA_FACE_KISS_WINK_HEART                   ");
    ImGui::Text("\xef\x96\x99""        ICON_FA_FACE_LAUGH                             ");
    ImGui::Text("\xef\x96\x9a""        ICON_FA_FACE_LAUGH_BEAM                        ");
    ImGui::Text("\xef\x96\x9b""        ICON_FA_FACE_LAUGH_SQUINT                      ");
    ImGui::Text("\xef\x96\x9c""        ICON_FA_FACE_LAUGH_WINK                        ");
    ImGui::Text("\xef\x84\x9a""        ICON_FA_FACE_MEH                               ");
    ImGui::Text("\xef\x96\xa4""        ICON_FA_FACE_MEH_BLANK                         ");
    ImGui::Text("\xef\x96\xa5""        ICON_FA_FACE_ROLLING_EYES                      ");
    ImGui::Text("\xef\x96\xb3""        ICON_FA_FACE_SAD_CRY                           ");
    ImGui::Text("\xef\x96\xb4""        ICON_FA_FACE_SAD_TEAR                          ");
    ImGui::Text("\xef\x84\x98""        ICON_FA_FACE_SMILE                             ");
    ImGui::Text("\xef\x96\xb8""        ICON_FA_FACE_SMILE_BEAM                        ");
    ImGui::Text("\xef\x93\x9a""        ICON_FA_FACE_SMILE_WINK                        ");
    ImGui::Text("\xef\x97\x82""        ICON_FA_FACE_SURPRISE                          ");
    ImGui::Text("\xef\x97\x88""        ICON_FA_FACE_TIRED                             ");
    ImGui::Text("\xef\xa1\xa3""        ICON_FA_FAN                                    ");
    ImGui::Text("\xee\x80\x85""        ICON_FA_FAUCET                                 ");
    ImGui::Text("\xee\x80\x86""        ICON_FA_FAUCET_DRIP                            ");
    ImGui::Text("\xef\x86\xac""        ICON_FA_FAX                                    ");
    ImGui::Text("\xef\x94\xad""        ICON_FA_FEATHER                                ");
    ImGui::Text("\xef\x95\xab""        ICON_FA_FEATHER_POINTED                        ");
    ImGui::Text("\xee\x93\xaa""        ICON_FA_FERRY                                  ");
    ImGui::Text("\xef\x85\x9b""        ICON_FA_FILE                                   ");
    ImGui::Text("\xef\x95\xad""        ICON_FA_FILE_ARROW_DOWN                        ");
    ImGui::Text("\xef\x95\xb4""        ICON_FA_FILE_ARROW_UP                          ");
    ImGui::Text("\xef\x87\x87""        ICON_FA_FILE_AUDIO                             ");
    ImGui::Text("\xee\x96\xa0""        ICON_FA_FILE_CIRCLE_CHECK                      ");
    ImGui::Text("\xee\x93\xab""        ICON_FA_FILE_CIRCLE_EXCLAMATION                ");
    ImGui::Text("\xee\x93\xad""        ICON_FA_FILE_CIRCLE_MINUS                      ");
    ImGui::Text("\xee\x92\x94""        ICON_FA_FILE_CIRCLE_PLUS                       ");
    ImGui::Text("\xee\x93\xaf""        ICON_FA_FILE_CIRCLE_QUESTION                   ");
    ImGui::Text("\xee\x96\xa1""        ICON_FA_FILE_CIRCLE_XMARK                      ");
    ImGui::Text("\xef\x87\x89""        ICON_FA_FILE_CODE                              ");
    ImGui::Text("\xef\x95\xac""        ICON_FA_FILE_CONTRACT                          ");
    ImGui::Text("\xef\x9b\x9d""        ICON_FA_FILE_CSV                               ");
    ImGui::Text("\xef\x87\x83""        ICON_FA_FILE_EXCEL                             ");
    ImGui::Text("\xef\x95\xae""        ICON_FA_FILE_EXPORT                            ");
    ImGui::Text("\xef\x87\x85""        ICON_FA_FILE_IMAGE                             ");
    ImGui::Text("\xef\x95\xaf""        ICON_FA_FILE_IMPORT                            ");
    ImGui::Text("\xef\x95\xb0""        ICON_FA_FILE_INVOICE                           ");
    ImGui::Text("\xef\x95\xb1""        ICON_FA_FILE_INVOICE_DOLLAR                    ");
    ImGui::Text("\xef\x85\x9c""        ICON_FA_FILE_LINES                             ");
    ImGui::Text("\xef\x91\xb7""        ICON_FA_FILE_MEDICAL                           ");
    ImGui::Text("\xef\x87\x81""        ICON_FA_FILE_PDF                               ");
    ImGui::Text("\xef\x8c\x9c""        ICON_FA_FILE_PEN                               ");
    ImGui::Text("\xef\x87\x84""        ICON_FA_FILE_POWERPOINT                        ");
    ImGui::Text("\xef\x95\xb2""        ICON_FA_FILE_PRESCRIPTION                      ");
    ImGui::Text("\xee\x93\xb0""        ICON_FA_FILE_SHIELD                            ");
    ImGui::Text("\xef\x95\xb3""        ICON_FA_FILE_SIGNATURE                         ");
    ImGui::Text("\xef\x87\x88""        ICON_FA_FILE_VIDEO                             ");
    ImGui::Text("\xef\x91\xb8""        ICON_FA_FILE_WAVEFORM                          ");
    ImGui::Text("\xef\x87\x82""        ICON_FA_FILE_WORD                              ");
    ImGui::Text("\xef\x87\x86""        ICON_FA_FILE_ZIPPER                            ");
    ImGui::Text("\xef\x95\xb5""        ICON_FA_FILL                                   ");
    ImGui::Text("\xef\x95\xb6""        ICON_FA_FILL_DRIP                              ");
    ImGui::Text("\xef\x80\x88""        ICON_FA_FILM                                   ");
    ImGui::Text("\xef\x82\xb0""        ICON_FA_FILTER                                 ");
    ImGui::Text("\xef\x99\xa2""        ICON_FA_FILTER_CIRCLE_DOLLAR                   ");
    ImGui::Text("\xee\x85\xbb""        ICON_FA_FILTER_CIRCLE_XMARK                    ");
    ImGui::Text("\xef\x95\xb7""        ICON_FA_FINGERPRINT                            ");
    ImGui::Text("\xef\x81\xad""        ICON_FA_FIRE                                   ");
    ImGui::Text("\xee\x93\xb1""        ICON_FA_FIRE_BURNER                            ");
    ImGui::Text("\xef\x84\xb4""        ICON_FA_FIRE_EXTINGUISHER                      ");
    ImGui::Text("\xef\x9f\xa4""        ICON_FA_FIRE_FLAME_CURVED                      ");
    ImGui::Text("\xef\x91\xaa""        ICON_FA_FIRE_FLAME_SIMPLE                      ");
    ImGui::Text("\xef\x95\xb8""        ICON_FA_FISH                                   ");
    ImGui::Text("\xee\x93\xb2""        ICON_FA_FISH_FINS                              ");
    ImGui::Text("\xef\x80\xa4""        ICON_FA_FLAG                                   ");
    ImGui::Text("\xef\x84\x9e""        ICON_FA_FLAG_CHECKERED                         ");
    ImGui::Text("\xef\x9d\x8d""        ICON_FA_FLAG_USA                               ");
    ImGui::Text("\xef\x83\x83""        ICON_FA_FLASK                                  ");
    ImGui::Text("\xee\x93\xb3""        ICON_FA_FLASK_VIAL                             ");
    ImGui::Text("\xef\x83\x87""        ICON_FA_FLOPPY_DISK                            ");
    ImGui::Text("\xee\x86\x84""        ICON_FA_FLORIN_SIGN                            ");
    ImGui::Text("\xef\x81\xbb""        ICON_FA_FOLDER                                 ");
    ImGui::Text("\xee\x86\x85""        ICON_FA_FOLDER_CLOSED                          ");
    ImGui::Text("\xef\x99\x9d""        ICON_FA_FOLDER_MINUS                           ");
    ImGui::Text("\xef\x81\xbc""        ICON_FA_FOLDER_OPEN                            ");
    ImGui::Text("\xef\x99\x9e""        ICON_FA_FOLDER_PLUS                            ");
    ImGui::Text("\xef\xa0\x82""        ICON_FA_FOLDER_TREE                            ");
    ImGui::Text("\xef\x80\xb1""        ICON_FA_FONT                                   ");
    ImGui::Text("\xef\x8a\xb4""        ICON_FA_FONT_AWESOME                           ");
    ImGui::Text("\xef\x91\x8e""        ICON_FA_FOOTBALL                               ");
    ImGui::Text("\xef\x81\x8e""        ICON_FA_FORWARD                                ");
    ImGui::Text("\xef\x81\x90""        ICON_FA_FORWARD_FAST                           ");
    ImGui::Text("\xef\x81\x91""        ICON_FA_FORWARD_STEP                           ");
    ImGui::Text("\xee\x86\x8f""        ICON_FA_FRANC_SIGN                             ");
    ImGui::Text("\xef\x94\xae""        ICON_FA_FROG                                   ");
    ImGui::Text("\xef\x87\xa3""        ICON_FA_FUTBOL                                 ");
    ImGui::Text("G"	       "        ICON_FA_G                                      ");
    ImGui::Text("\xef\x84\x9b""        ICON_FA_GAMEPAD                                ");
    ImGui::Text("\xef\x94\xaf""        ICON_FA_GAS_PUMP                               ");
    ImGui::Text("\xef\x98\xa4""        ICON_FA_GAUGE                                  ");
    ImGui::Text("\xef\x98\xa5""        ICON_FA_GAUGE_HIGH                             ");
    ImGui::Text("\xef\x98\xa9""        ICON_FA_GAUGE_SIMPLE                           ");
    ImGui::Text("\xef\x98\xaa""        ICON_FA_GAUGE_SIMPLE_HIGH                      ");
    ImGui::Text("\xef\x83\xa3""        ICON_FA_GAVEL                                  ");
    ImGui::Text("\xef\x80\x93""        ICON_FA_GEAR                                   ");
    ImGui::Text("\xef\x82\x85""        ICON_FA_GEARS                                  ");
    ImGui::Text("\xef\x8e\xa5""        ICON_FA_GEM                                    ");
    ImGui::Text("\xef\x88\xad""        ICON_FA_GENDERLESS                             ");
    ImGui::Text("\xef\x9b\xa2""        ICON_FA_GHOST                                  ");
    ImGui::Text("\xef\x81\xab""        ICON_FA_GIFT                                   ");
    ImGui::Text("\xef\x9e\x9c""        ICON_FA_GIFTS                                  ");
    ImGui::Text("\xee\x93\xb4""        ICON_FA_GLASS_WATER                            ");
    ImGui::Text("\xee\x93\xb5""        ICON_FA_GLASS_WATER_DROPLET                    ");
    ImGui::Text("\xef\x94\xb0""        ICON_FA_GLASSES                                ");
    ImGui::Text("\xef\x82\xac""        ICON_FA_GLOBE                                  ");
    ImGui::Text("\xef\x91\x90""        ICON_FA_GOLF_BALL_TEE                          ");
    ImGui::Text("\xef\x99\xa4""        ICON_FA_GOPURAM                                ");
    ImGui::Text("\xef\x86\x9d""        ICON_FA_GRADUATION_CAP                         ");
    ImGui::Text(">"	       "        ICON_FA_GREATER_THAN                           ");
    ImGui::Text("\xef\x94\xb2""        ICON_FA_GREATER_THAN_EQUAL                     ");
    ImGui::Text("\xef\x96\x8d""        ICON_FA_GRIP                                   ");
    ImGui::Text("\xef\x9e\xa4""        ICON_FA_GRIP_LINES                             ");
    ImGui::Text("\xef\x9e\xa5""        ICON_FA_GRIP_LINES_VERTICAL                    ");
    ImGui::Text("\xef\x96\x8e""        ICON_FA_GRIP_VERTICAL                          ");
    ImGui::Text("\xee\x93\xb6""        ICON_FA_GROUP_ARROWS_ROTATE                    ");
    ImGui::Text("\xee\x86\x9a""        ICON_FA_GUARANI_SIGN                           ");
    ImGui::Text("\xef\x9e\xa6""        ICON_FA_GUITAR                                 ");
    ImGui::Text("\xee\x86\x9b""        ICON_FA_GUN                                    ");
    ImGui::Text("H"	       "        ICON_FA_H                                      ");
    ImGui::Text("\xef\x9b\xa3""        ICON_FA_HAMMER                                 ");
    ImGui::Text("\xef\x99\xa5""        ICON_FA_HAMSA                                  ");
    ImGui::Text("\xef\x89\x96""        ICON_FA_HAND                                   ");
    ImGui::Text("\xef\x89\x95""        ICON_FA_HAND_BACK_FIST                         ");
    ImGui::Text("\xef\x91\xa1""        ICON_FA_HAND_DOTS                              ");
    ImGui::Text("\xef\x9b\x9e""        ICON_FA_HAND_FIST                              ");
    ImGui::Text("\xef\x92\xbd""        ICON_FA_HAND_HOLDING                           ");
    ImGui::Text("\xef\x93\x80""        ICON_FA_HAND_HOLDING_DOLLAR                    ");
    ImGui::Text("\xef\x93\x81""        ICON_FA_HAND_HOLDING_DROPLET                   ");
    ImGui::Text("\xee\x93\xb7""        ICON_FA_HAND_HOLDING_HAND                      ");
    ImGui::Text("\xef\x92\xbe""        ICON_FA_HAND_HOLDING_HEART                     ");
    ImGui::Text("\xee\x81\x9c""        ICON_FA_HAND_HOLDING_MEDICAL                   ");
    ImGui::Text("\xef\x89\x98""        ICON_FA_HAND_LIZARD                            ");
    ImGui::Text("\xef\xa0\x86""        ICON_FA_HAND_MIDDLE_FINGER                     ");
    ImGui::Text("\xef\x89\x9b""        ICON_FA_HAND_PEACE                             ");
    ImGui::Text("\xef\x82\xa7""        ICON_FA_HAND_POINT_DOWN                        ");
    ImGui::Text("\xef\x82\xa5""        ICON_FA_HAND_POINT_LEFT                        ");
    ImGui::Text("\xef\x82\xa4""        ICON_FA_HAND_POINT_RIGHT                       ");
    ImGui::Text("\xef\x82\xa6""        ICON_FA_HAND_POINT_UP                          ");
    ImGui::Text("\xef\x89\x9a""        ICON_FA_HAND_POINTER                           ");
    ImGui::Text("\xef\x89\x97""        ICON_FA_HAND_SCISSORS                          ");
    ImGui::Text("\xee\x81\x9d""        ICON_FA_HAND_SPARKLES                          ");
    ImGui::Text("\xef\x89\x99""        ICON_FA_HAND_SPOCK                             ");
    ImGui::Text("\xee\x93\xb8""        ICON_FA_HANDCUFFS                              ");
    ImGui::Text("\xef\x8a\xa7""        ICON_FA_HANDS                                  ");
    ImGui::Text("\xef\x8a\xa3""        ICON_FA_HANDS_ASL_INTERPRETING                 ");
    ImGui::Text("\xee\x93\xb9""        ICON_FA_HANDS_BOUND                            ");
    ImGui::Text("\xee\x81\x9e""        ICON_FA_HANDS_BUBBLES                          ");
    ImGui::Text("\xee\x86\xa8""        ICON_FA_HANDS_CLAPPING                         ");
    ImGui::Text("\xef\x93\x82""        ICON_FA_HANDS_HOLDING                          ");
    ImGui::Text("\xee\x93\xba""        ICON_FA_HANDS_HOLDING_CHILD                    ");
    ImGui::Text("\xee\x93\xbb""        ICON_FA_HANDS_HOLDING_CIRCLE                   ");
    ImGui::Text("\xef\x9a\x84""        ICON_FA_HANDS_PRAYING                          ");
    ImGui::Text("\xef\x8a\xb5""        ICON_FA_HANDSHAKE                              ");
    ImGui::Text("\xef\x93\x84""        ICON_FA_HANDSHAKE_ANGLE                        ");
    ImGui::Text("\xef\x93\x86""        ICON_FA_HANDSHAKE_SIMPLE                       ");
    ImGui::Text("\xee\x81\x9f""        ICON_FA_HANDSHAKE_SIMPLE_SLASH                 ");
    ImGui::Text("\xee\x81\xa0""        ICON_FA_HANDSHAKE_SLASH                        ");
    ImGui::Text("\xef\x9b\xa6""        ICON_FA_HANUKIAH                               ");
    ImGui::Text("\xef\x82\xa0""        ICON_FA_HARD_DRIVE                             ");
    ImGui::Text("#"	       "        ICON_FA_HASHTAG                                ");
    ImGui::Text("\xef\xa3\x80""        ICON_FA_HAT_COWBOY                             ");
    ImGui::Text("\xef\xa3\x81""        ICON_FA_HAT_COWBOY_SIDE                        ");
    ImGui::Text("\xef\x9b\xa8""        ICON_FA_HAT_WIZARD                             ");
    ImGui::Text("\xee\x81\xa1""        ICON_FA_HEAD_SIDE_COUGH                        ");
    ImGui::Text("\xee\x81\xa2""        ICON_FA_HEAD_SIDE_COUGH_SLASH                  ");
    ImGui::Text("\xee\x81\xa3""        ICON_FA_HEAD_SIDE_MASK                         ");
    ImGui::Text("\xee\x81\xa4""        ICON_FA_HEAD_SIDE_VIRUS                        ");
    ImGui::Text("\xef\x87\x9c""        ICON_FA_HEADING                                ");
    ImGui::Text("\xef\x80\xa5""        ICON_FA_HEADPHONES                             ");
    ImGui::Text("\xef\x96\x8f""        ICON_FA_HEADPHONES_SIMPLE                      ");
    ImGui::Text("\xef\x96\x90""        ICON_FA_HEADSET                                ");
    ImGui::Text("\xef\x80\x84""        ICON_FA_HEART                                  ");
    ImGui::Text("\xee\x93\xbc""        ICON_FA_HEART_CIRCLE_BOLT                      ");
    ImGui::Text("\xee\x93\xbd""        ICON_FA_HEART_CIRCLE_CHECK                     ");
    ImGui::Text("\xee\x93\xbe""        ICON_FA_HEART_CIRCLE_EXCLAMATION               ");
    ImGui::Text("\xee\x93\xbf""        ICON_FA_HEART_CIRCLE_MINUS                     ");
    ImGui::Text("\xee\x94\x80""        ICON_FA_HEART_CIRCLE_PLUS                      ");
    ImGui::Text("\xee\x94\x81""        ICON_FA_HEART_CIRCLE_XMARK                     ");
    ImGui::Text("\xef\x9e\xa9""        ICON_FA_HEART_CRACK                            ");
    ImGui::Text("\xef\x88\x9e""        ICON_FA_HEART_PULSE                            ");
    ImGui::Text("\xef\x94\xb3""        ICON_FA_HELICOPTER                             ");
    ImGui::Text("\xee\x94\x82""        ICON_FA_HELICOPTER_SYMBOL                      ");
    ImGui::Text("\xef\xa0\x87""        ICON_FA_HELMET_SAFETY                          ");
    ImGui::Text("\xee\x94\x83""        ICON_FA_HELMET_UN                              ");
    ImGui::Text("\xef\x96\x91""        ICON_FA_HIGHLIGHTER                            ");
    ImGui::Text("\xee\x94\x87""        ICON_FA_HILL_AVALANCHE                         ");
    ImGui::Text("\xee\x94\x88""        ICON_FA_HILL_ROCKSLIDE                         ");
    ImGui::Text("\xef\x9b\xad""        ICON_FA_HIPPO                                  ");
    ImGui::Text("\xef\x91\x93""        ICON_FA_HOCKEY_PUCK                            ");
    ImGui::Text("\xef\x9e\xaa""        ICON_FA_HOLLY_BERRY                            ");
    ImGui::Text("\xef\x9b\xb0""        ICON_FA_HORSE                                  ");
    ImGui::Text("\xef\x9e\xab""        ICON_FA_HORSE_HEAD                             ");
    ImGui::Text("\xef\x83\xb8""        ICON_FA_HOSPITAL                               ");
    ImGui::Text("\xef\xa0\x8d""        ICON_FA_HOSPITAL_USER                          ");
    ImGui::Text("\xef\x96\x93""        ICON_FA_HOT_TUB_PERSON                         ");
    ImGui::Text("\xef\xa0\x8f""        ICON_FA_HOTDOG                                 ");
    ImGui::Text("\xef\x96\x94""        ICON_FA_HOTEL                                  ");
    ImGui::Text("\xef\x89\x94""        ICON_FA_HOURGLASS                              ");
    ImGui::Text("\xef\x89\x93""        ICON_FA_HOURGLASS_END                          ");
    ImGui::Text("\xef\x89\x92""        ICON_FA_HOURGLASS_HALF                         ");
    ImGui::Text("\xef\x89\x91""        ICON_FA_HOURGLASS_START                        ");
    ImGui::Text("\xef\x80\x95""        ICON_FA_HOUSE                                  ");
    ImGui::Text("\xee\x8e\xaf""        ICON_FA_HOUSE_CHIMNEY                          ");
    ImGui::Text("\xef\x9b\xb1""        ICON_FA_HOUSE_CHIMNEY_CRACK                    ");
    ImGui::Text("\xef\x9f\xb2""        ICON_FA_HOUSE_CHIMNEY_MEDICAL                  ");
    ImGui::Text("\xee\x81\xa5""        ICON_FA_HOUSE_CHIMNEY_USER                     ");
    ImGui::Text("\xee\x80\x8d""        ICON_FA_HOUSE_CHIMNEY_WINDOW                   ");
    ImGui::Text("\xee\x94\x89""        ICON_FA_HOUSE_CIRCLE_CHECK                     ");
    ImGui::Text("\xee\x94\x8a""        ICON_FA_HOUSE_CIRCLE_EXCLAMATION               ");
    ImGui::Text("\xee\x94\x8b""        ICON_FA_HOUSE_CIRCLE_XMARK                     ");
    ImGui::Text("\xee\x8e\xb1""        ICON_FA_HOUSE_CRACK                            ");
    ImGui::Text("\xee\x94\x8c""        ICON_FA_HOUSE_FIRE                             ");
    ImGui::Text("\xee\x94\x8d""        ICON_FA_HOUSE_FLAG                             ");
    ImGui::Text("\xee\x94\x8e""        ICON_FA_HOUSE_FLOOD_WATER                      ");
    ImGui::Text("\xee\x94\x8f""        ICON_FA_HOUSE_FLOOD_WATER_CIRCLE_ARROW_RIGHT   ");
    ImGui::Text("\xee\x81\xa6""        ICON_FA_HOUSE_LAPTOP                           ");
    ImGui::Text("\xee\x94\x90""        ICON_FA_HOUSE_LOCK                             ");
    ImGui::Text("\xee\x8e\xb2""        ICON_FA_HOUSE_MEDICAL                          ");
    ImGui::Text("\xee\x94\x91""        ICON_FA_HOUSE_MEDICAL_CIRCLE_CHECK             ");
    ImGui::Text("\xee\x94\x92""        ICON_FA_HOUSE_MEDICAL_CIRCLE_EXCLAMATION       ");
    ImGui::Text("\xee\x94\x93""        ICON_FA_HOUSE_MEDICAL_CIRCLE_XMARK             ");
    ImGui::Text("\xee\x94\x94""        ICON_FA_HOUSE_MEDICAL_FLAG                     ");
    ImGui::Text("\xee\x80\x92""        ICON_FA_HOUSE_SIGNAL                           ");
    ImGui::Text("\xee\x94\x95""        ICON_FA_HOUSE_TSUNAMI                          ");
    ImGui::Text("\xee\x86\xb0""        ICON_FA_HOUSE_USER                             ");
    ImGui::Text("\xef\x9b\xb2""        ICON_FA_HRYVNIA_SIGN                           ");
    ImGui::Text("\xef\x9d\x91""        ICON_FA_HURRICANE                              ");
    ImGui::Text("I"	       "        ICON_FA_I                                      ");
    ImGui::Text("\xef\x89\x86""        ICON_FA_I_CURSOR                               ");
    ImGui::Text("\xef\xa0\x90""        ICON_FA_ICE_CREAM                              ");
    ImGui::Text("\xef\x9e\xad""        ICON_FA_ICICLES                                ");
    ImGui::Text("\xef\xa1\xad""        ICON_FA_ICONS                                  ");
    ImGui::Text("\xef\x8b\x81""        ICON_FA_ID_BADGE                               ");
    ImGui::Text("\xef\x8b\x82""        ICON_FA_ID_CARD                                ");
    ImGui::Text("\xef\x91\xbf""        ICON_FA_ID_CARD_CLIP                           ");
    ImGui::Text("\xef\x9e\xae""        ICON_FA_IGLOO                                  ");
    ImGui::Text("\xef\x80\xbe""        ICON_FA_IMAGE                                  ");
    ImGui::Text("\xef\x8f\xa0""        ICON_FA_IMAGE_PORTRAIT                         ");
    ImGui::Text("\xef\x8c\x82""        ICON_FA_IMAGES                                 ");
    ImGui::Text("\xef\x80\x9c""        ICON_FA_INBOX                                  ");
    ImGui::Text("\xef\x80\xbc""        ICON_FA_INDENT                                 ");
    ImGui::Text("\xee\x86\xbc""        ICON_FA_INDIAN_RUPEE_SIGN                      ");
    ImGui::Text("\xef\x89\xb5""        ICON_FA_INDUSTRY                               ");
    ImGui::Text("\xef\x94\xb4""        ICON_FA_INFINITY                               ");
    ImGui::Text("\xef\x84\xa9""        ICON_FA_INFO                                   ");
    ImGui::Text("\xef\x80\xb3""        ICON_FA_ITALIC                                 ");
    ImGui::Text("J"	       "        ICON_FA_J                                      ");
    ImGui::Text("\xee\x94\x96""        ICON_FA_JAR                                    ");
    ImGui::Text("\xee\x94\x97""        ICON_FA_JAR_WHEAT                              ");
    ImGui::Text("\xef\x99\xa9""        ICON_FA_JEDI                                   ");
    ImGui::Text("\xef\x83\xbb""        ICON_FA_JET_FIGHTER                            ");
    ImGui::Text("\xee\x94\x98""        ICON_FA_JET_FIGHTER_UP                         ");
    ImGui::Text("\xef\x96\x95""        ICON_FA_JOINT                                  ");
    ImGui::Text("\xee\x94\x99""        ICON_FA_JUG_DETERGENT                          ");
    ImGui::Text("K"	       "        ICON_FA_K                                      ");
    ImGui::Text("\xef\x99\xab""        ICON_FA_KAABA                                  ");
    ImGui::Text("\xef\x82\x84""        ICON_FA_KEY                                    ");
    ImGui::Text("\xef\x84\x9c""        ICON_FA_KEYBOARD                               ");
    ImGui::Text("\xef\x99\xad""        ICON_FA_KHANDA                                 ");
    ImGui::Text("\xee\x87\x84""        ICON_FA_KIP_SIGN                               ");
    ImGui::Text("\xef\x91\xb9""        ICON_FA_KIT_MEDICAL                            ");
    ImGui::Text("\xee\x94\x9a""        ICON_FA_KITCHEN_SET                            ");
    ImGui::Text("\xef\x94\xb5""        ICON_FA_KIWI_BIRD                              ");
    ImGui::Text("L"	       "        ICON_FA_L                                      ");
    ImGui::Text("\xee\x94\x9b""        ICON_FA_LAND_MINE_ON                           ");
    ImGui::Text("\xef\x99\xaf""        ICON_FA_LANDMARK                               ");
    ImGui::Text("\xef\x9d\x92""        ICON_FA_LANDMARK_DOME                          ");
    ImGui::Text("\xee\x94\x9c""        ICON_FA_LANDMARK_FLAG                          ");
    ImGui::Text("\xef\x86\xab""        ICON_FA_LANGUAGE                               ");
    ImGui::Text("\xef\x84\x89""        ICON_FA_LAPTOP                                 ");
    ImGui::Text("\xef\x97\xbc""        ICON_FA_LAPTOP_CODE                            ");
    ImGui::Text("\xee\x94\x9d""        ICON_FA_LAPTOP_FILE                            ");
    ImGui::Text("\xef\xa0\x92""        ICON_FA_LAPTOP_MEDICAL                         ");
    ImGui::Text("\xee\x87\x88""        ICON_FA_LARI_SIGN                              ");
    ImGui::Text("\xef\x97\xbd""        ICON_FA_LAYER_GROUP                            ");
    ImGui::Text("\xef\x81\xac""        ICON_FA_LEAF                                   ");
    ImGui::Text("\xef\x8c\x8a""        ICON_FA_LEFT_LONG                              ");
    ImGui::Text("\xef\x8c\xb7""        ICON_FA_LEFT_RIGHT                             ");
    ImGui::Text("\xef\x82\x94""        ICON_FA_LEMON                                  ");
    ImGui::Text("<"	       "        ICON_FA_LESS_THAN                              ");
    ImGui::Text("\xef\x94\xb7""        ICON_FA_LESS_THAN_EQUAL                        ");
    ImGui::Text("\xef\x87\x8d""        ICON_FA_LIFE_RING                              ");
    ImGui::Text("\xef\x83\xab""        ICON_FA_LIGHTBULB                              ");
    ImGui::Text("\xee\x94\x9e""        ICON_FA_LINES_LEANING                          ");
    ImGui::Text("\xef\x83\x81""        ICON_FA_LINK                                   ");
    ImGui::Text("\xef\x84\xa7""        ICON_FA_LINK_SLASH                             ");
    ImGui::Text("\xef\x86\x95""        ICON_FA_LIRA_SIGN                              ");
    ImGui::Text("\xef\x80\xba""        ICON_FA_LIST                                   ");
    ImGui::Text("\xef\x82\xae""        ICON_FA_LIST_CHECK                             ");
    ImGui::Text("\xef\x83\x8b""        ICON_FA_LIST_OL                                ");
    ImGui::Text("\xef\x83\x8a""        ICON_FA_LIST_UL                                ");
    ImGui::Text("\xee\x87\x93""        ICON_FA_LITECOIN_SIGN                          ");
    ImGui::Text("\xef\x84\xa4""        ICON_FA_LOCATION_ARROW                         ");
    ImGui::Text("\xef\x98\x81""        ICON_FA_LOCATION_CROSSHAIRS                    ");
    ImGui::Text("\xef\x8f\x85""        ICON_FA_LOCATION_DOT                           ");
    ImGui::Text("\xef\x81\x81""        ICON_FA_LOCATION_PIN                           ");
    ImGui::Text("\xee\x94\x9f""        ICON_FA_LOCATION_PIN_LOCK                      ");
    ImGui::Text("\xef\x80\xa3""        ICON_FA_LOCK                                   ");
    ImGui::Text("\xef\x8f\x81""        ICON_FA_LOCK_OPEN                              ");
    ImGui::Text("\xee\x94\xa0""        ICON_FA_LOCUST                                 ");
    ImGui::Text("\xef\x98\x84""        ICON_FA_LUNGS                                  ");
    ImGui::Text("\xee\x81\xa7""        ICON_FA_LUNGS_VIRUS                            ");
    ImGui::Text("M"	       "        ICON_FA_M                                      ");
    ImGui::Text("\xef\x81\xb6""        ICON_FA_MAGNET                                 ");
    ImGui::Text("\xef\x80\x82""        ICON_FA_MAGNIFYING_GLASS                       ");
    ImGui::Text("\xee\x94\xa1""        ICON_FA_MAGNIFYING_GLASS_ARROW_RIGHT           ");
    ImGui::Text("\xee\x94\xa2""        ICON_FA_MAGNIFYING_GLASS_CHART                 ");
    ImGui::Text("\xef\x9a\x88""        ICON_FA_MAGNIFYING_GLASS_DOLLAR                ");
    ImGui::Text("\xef\x9a\x89""        ICON_FA_MAGNIFYING_GLASS_LOCATION              ");
    ImGui::Text("\xef\x80\x90""        ICON_FA_MAGNIFYING_GLASS_MINUS                 ");
    ImGui::Text("\xef\x80\x8e""        ICON_FA_MAGNIFYING_GLASS_PLUS                  ");
    ImGui::Text("\xee\x87\x95""        ICON_FA_MANAT_SIGN                             ");
    ImGui::Text("\xef\x89\xb9""        ICON_FA_MAP                                    ");
    ImGui::Text("\xef\x96\x9f""        ICON_FA_MAP_LOCATION                           ");
    ImGui::Text("\xef\x96\xa0""        ICON_FA_MAP_LOCATION_DOT                       ");
    ImGui::Text("\xef\x89\xb6""        ICON_FA_MAP_PIN                                ");
    ImGui::Text("\xef\x96\xa1""        ICON_FA_MARKER                                 ");
    ImGui::Text("\xef\x88\xa2""        ICON_FA_MARS                                   ");
    ImGui::Text("\xef\x88\xa4""        ICON_FA_MARS_AND_VENUS                         ");
    ImGui::Text("\xee\x94\xa3""        ICON_FA_MARS_AND_VENUS_BURST                   ");
    ImGui::Text("\xef\x88\xa7""        ICON_FA_MARS_DOUBLE                            ");
    ImGui::Text("\xef\x88\xa9""        ICON_FA_MARS_STROKE                            ");
    ImGui::Text("\xef\x88\xab""        ICON_FA_MARS_STROKE_RIGHT                      ");
    ImGui::Text("\xef\x88\xaa""        ICON_FA_MARS_STROKE_UP                         ");
    ImGui::Text("\xef\x95\xbb""        ICON_FA_MARTINI_GLASS                          ");
    ImGui::Text("\xef\x95\xa1""        ICON_FA_MARTINI_GLASS_CITRUS                   ");
    ImGui::Text("\xef\x80\x80""        ICON_FA_MARTINI_GLASS_EMPTY                    ");
    ImGui::Text("\xef\x9b\xba""        ICON_FA_MASK                                   ");
    ImGui::Text("\xee\x87\x97""        ICON_FA_MASK_FACE                              ");
    ImGui::Text("\xee\x94\xa4""        ICON_FA_MASK_VENTILATOR                        ");
    ImGui::Text("\xef\x98\xb0""        ICON_FA_MASKS_THEATER                          ");
    ImGui::Text("\xee\x94\xa5""        ICON_FA_MATTRESS_PILLOW                        ");
    ImGui::Text("\xef\x8c\x9e""        ICON_FA_MAXIMIZE                               ");
    ImGui::Text("\xef\x96\xa2""        ICON_FA_MEDAL                                  ");
    ImGui::Text("\xef\x94\xb8""        ICON_FA_MEMORY                                 ");
    ImGui::Text("\xef\x99\xb6""        ICON_FA_MENORAH                                ");
    ImGui::Text("\xef\x88\xa3""        ICON_FA_MERCURY                                ");
    ImGui::Text("\xef\x89\xba""        ICON_FA_MESSAGE                                ");
    ImGui::Text("\xef\x9d\x93""        ICON_FA_METEOR                                 ");
    ImGui::Text("\xef\x8b\x9b""        ICON_FA_MICROCHIP                              ");
    ImGui::Text("\xef\x84\xb0""        ICON_FA_MICROPHONE                             ");
    ImGui::Text("\xef\x8f\x89""        ICON_FA_MICROPHONE_LINES                       ");
    ImGui::Text("\xef\x94\xb9""        ICON_FA_MICROPHONE_LINES_SLASH                 ");
    ImGui::Text("\xef\x84\xb1""        ICON_FA_MICROPHONE_SLASH                       ");
    ImGui::Text("\xef\x98\x90""        ICON_FA_MICROSCOPE                             ");
    ImGui::Text("\xee\x87\xad""        ICON_FA_MILL_SIGN                              ");
    ImGui::Text("\xef\x9e\x8c""        ICON_FA_MINIMIZE                               ");
    ImGui::Text("\xef\x81\xa8""        ICON_FA_MINUS                                  ");
    ImGui::Text("\xef\x9e\xb5""        ICON_FA_MITTEN                                 ");
    ImGui::Text("\xef\x8f\x8e""        ICON_FA_MOBILE                                 ");
    ImGui::Text("\xef\x84\x8b""        ICON_FA_MOBILE_BUTTON                          ");
    ImGui::Text("\xee\x94\xa7""        ICON_FA_MOBILE_RETRO                           ");
    ImGui::Text("\xef\x8f\x8f""        ICON_FA_MOBILE_SCREEN                          ");
    ImGui::Text("\xef\x8f\x8d""        ICON_FA_MOBILE_SCREEN_BUTTON                   ");
    ImGui::Text("\xef\x83\x96""        ICON_FA_MONEY_BILL                             ");
    ImGui::Text("\xef\x8f\x91""        ICON_FA_MONEY_BILL_1                           ");
    ImGui::Text("\xef\x94\xbb""        ICON_FA_MONEY_BILL_1_WAVE                      ");
    ImGui::Text("\xee\x94\xa8""        ICON_FA_MONEY_BILL_TRANSFER                    ");
    ImGui::Text("\xee\x94\xa9""        ICON_FA_MONEY_BILL_TREND_UP                    ");
    ImGui::Text("\xef\x94\xba""        ICON_FA_MONEY_BILL_WAVE                        ");
    ImGui::Text("\xee\x94\xaa""        ICON_FA_MONEY_BILL_WHEAT                       ");
    ImGui::Text("\xee\x87\xb3""        ICON_FA_MONEY_BILLS                            ");
    ImGui::Text("\xef\x94\xbc""        ICON_FA_MONEY_CHECK                            ");
    ImGui::Text("\xef\x94\xbd""        ICON_FA_MONEY_CHECK_DOLLAR                     ");
    ImGui::Text("\xef\x96\xa6""        ICON_FA_MONUMENT                               ");
    ImGui::Text("\xef\x86\x86""        ICON_FA_MOON                                   ");
    ImGui::Text("\xef\x96\xa7""        ICON_FA_MORTAR_PESTLE                          ");
    ImGui::Text("\xef\x99\xb8""        ICON_FA_MOSQUE                                 ");
    ImGui::Text("\xee\x94\xab""        ICON_FA_MOSQUITO                               ");
    ImGui::Text("\xee\x94\xac""        ICON_FA_MOSQUITO_NET                           ");
    ImGui::Text("\xef\x88\x9c""        ICON_FA_MOTORCYCLE                             ");
    ImGui::Text("\xee\x94\xad""        ICON_FA_MOUND                                  ");
    ImGui::Text("\xef\x9b\xbc""        ICON_FA_MOUNTAIN                               ");
    ImGui::Text("\xee\x94\xae""        ICON_FA_MOUNTAIN_CITY                          ");
    ImGui::Text("\xee\x94\xaf""        ICON_FA_MOUNTAIN_SUN                           ");
    ImGui::Text("\xef\x9e\xb6""        ICON_FA_MUG_HOT                                ");
    ImGui::Text("\xef\x83\xb4""        ICON_FA_MUG_SAUCER                             ");
    ImGui::Text("\xef\x80\x81""        ICON_FA_MUSIC                                  ");
    ImGui::Text("N"	       "        ICON_FA_N                                      ");
    ImGui::Text("\xee\x87\xb6""        ICON_FA_NAIRA_SIGN                             ");
    ImGui::Text("\xef\x9b\xbf""        ICON_FA_NETWORK_WIRED                          ");
    ImGui::Text("\xef\x88\xac""        ICON_FA_NEUTER                                 ");
    ImGui::Text("\xef\x87\xaa""        ICON_FA_NEWSPAPER                              ");
    ImGui::Text("\xef\x94\xbe""        ICON_FA_NOT_EQUAL                              ");
    ImGui::Text("\xee\x87\xbe""        ICON_FA_NOTDEF                                 ");
    ImGui::Text("\xef\x89\x89""        ICON_FA_NOTE_STICKY                            ");
    ImGui::Text("\xef\x92\x81""        ICON_FA_NOTES_MEDICAL                          ");
    ImGui::Text("O"	       "        ICON_FA_O                                      ");
    ImGui::Text("\xef\x89\x87""        ICON_FA_OBJECT_GROUP                           ");
    ImGui::Text("\xef\x89\x88""        ICON_FA_OBJECT_UNGROUP                         ");
    ImGui::Text("\xef\x98\x93""        ICON_FA_OIL_CAN                                ");
    ImGui::Text("\xee\x94\xb2""        ICON_FA_OIL_WELL                               ");
    ImGui::Text("\xef\x99\xb9""        ICON_FA_OM                                     ");
    ImGui::Text("\xef\x9c\x80""        ICON_FA_OTTER                                  ");
    ImGui::Text("\xef\x80\xbb""        ICON_FA_OUTDENT                                ");
    ImGui::Text("P"	       "        ICON_FA_P                                      ");
    ImGui::Text("\xef\xa0\x95""        ICON_FA_PAGER                                  ");
    ImGui::Text("\xef\x96\xaa""        ICON_FA_PAINT_ROLLER                           ");
    ImGui::Text("\xef\x87\xbc""        ICON_FA_PAINTBRUSH                             ");
    ImGui::Text("\xef\x94\xbf""        ICON_FA_PALETTE                                ");
    ImGui::Text("\xef\x92\x82""        ICON_FA_PALLET                                 ");
    ImGui::Text("\xee\x88\x89""        ICON_FA_PANORAMA                               ");
    ImGui::Text("\xef\x87\x98""        ICON_FA_PAPER_PLANE                            ");
    ImGui::Text("\xef\x83\x86""        ICON_FA_PAPERCLIP                              ");
    ImGui::Text("\xef\x93\x8d""        ICON_FA_PARACHUTE_BOX                          ");
    ImGui::Text("\xef\x87\x9d""        ICON_FA_PARAGRAPH                              ");
    ImGui::Text("\xef\x96\xab""        ICON_FA_PASSPORT                               ");
    ImGui::Text("\xef\x83\xaa""        ICON_FA_PASTE                                  ");
    ImGui::Text("\xef\x81\x8c""        ICON_FA_PAUSE                                  ");
    ImGui::Text("\xef\x86\xb0""        ICON_FA_PAW                                    ");
    ImGui::Text("\xef\x99\xbc""        ICON_FA_PEACE                                  ");
    ImGui::Text("\xef\x8c\x84""        ICON_FA_PEN                                    ");
    ImGui::Text("\xef\x8c\x85""        ICON_FA_PEN_CLIP                               ");
    ImGui::Text("\xef\x96\xac""        ICON_FA_PEN_FANCY                              ");
    ImGui::Text("\xef\x96\xad""        ICON_FA_PEN_NIB                                ");
    ImGui::Text("\xef\x96\xae""        ICON_FA_PEN_RULER                              ");
    ImGui::Text("\xef\x81\x84""        ICON_FA_PEN_TO_SQUARE                          ");
    ImGui::Text("\xef\x8c\x83""        ICON_FA_PENCIL                                 ");
    ImGui::Text("\xee\x81\xa8""        ICON_FA_PEOPLE_ARROWS                          ");
    ImGui::Text("\xef\x93\x8e""        ICON_FA_PEOPLE_CARRY_BOX                       ");
    ImGui::Text("\xee\x94\xb3""        ICON_FA_PEOPLE_GROUP                           ");
    ImGui::Text("\xee\x94\xb4""        ICON_FA_PEOPLE_LINE                            ");
    ImGui::Text("\xee\x94\xb5""        ICON_FA_PEOPLE_PULLING                         ");
    ImGui::Text("\xee\x94\xb6""        ICON_FA_PEOPLE_ROBBERY                         ");
    ImGui::Text("\xee\x94\xb7""        ICON_FA_PEOPLE_ROOF                            ");
    ImGui::Text("\xef\xa0\x96""        ICON_FA_PEPPER_HOT                             ");
    ImGui::Text("%"	       "        ICON_FA_PERCENT                                ");
    ImGui::Text("\xef\x86\x83""        ICON_FA_PERSON                                 ");
    ImGui::Text("\xee\x94\xb8""        ICON_FA_PERSON_ARROW_DOWN_TO_LINE              ");
    ImGui::Text("\xee\x94\xb9""        ICON_FA_PERSON_ARROW_UP_FROM_LINE              ");
    ImGui::Text("\xef\xa1\x8a""        ICON_FA_PERSON_BIKING                          ");
    ImGui::Text("\xef\x9d\x96""        ICON_FA_PERSON_BOOTH                           ");
    ImGui::Text("\xee\x94\xba""        ICON_FA_PERSON_BREASTFEEDING                   ");
    ImGui::Text("\xee\x94\xbb""        ICON_FA_PERSON_BURST                           ");
    ImGui::Text("\xee\x94\xbc""        ICON_FA_PERSON_CANE                            ");
    ImGui::Text("\xee\x94\xbd""        ICON_FA_PERSON_CHALKBOARD                      ");
    ImGui::Text("\xee\x94\xbe""        ICON_FA_PERSON_CIRCLE_CHECK                    ");
    ImGui::Text("\xee\x94\xbf""        ICON_FA_PERSON_CIRCLE_EXCLAMATION              ");
    ImGui::Text("\xee\x95\x80""        ICON_FA_PERSON_CIRCLE_MINUS                    ");
    ImGui::Text("\xee\x95\x81""        ICON_FA_PERSON_CIRCLE_PLUS                     ");
    ImGui::Text("\xee\x95\x82""        ICON_FA_PERSON_CIRCLE_QUESTION                 ");
    ImGui::Text("\xee\x95\x83""        ICON_FA_PERSON_CIRCLE_XMARK                    ");
    ImGui::Text("\xef\xa1\x9e""        ICON_FA_PERSON_DIGGING                         ");
    ImGui::Text("\xef\x91\xb0""        ICON_FA_PERSON_DOTS_FROM_LINE                  ");
    ImGui::Text("\xef\x86\x82""        ICON_FA_PERSON_DRESS                           ");
    ImGui::Text("\xee\x95\x84""        ICON_FA_PERSON_DRESS_BURST                     ");
    ImGui::Text("\xee\x95\x85""        ICON_FA_PERSON_DROWNING                        ");
    ImGui::Text("\xee\x95\x86""        ICON_FA_PERSON_FALLING                         ");
    ImGui::Text("\xee\x95\x87""        ICON_FA_PERSON_FALLING_BURST                   ");
    ImGui::Text("\xee\x95\x88""        ICON_FA_PERSON_HALF_DRESS                      ");
    ImGui::Text("\xee\x95\x89""        ICON_FA_PERSON_HARASSING                       ");
    ImGui::Text("\xef\x9b\xac""        ICON_FA_PERSON_HIKING                          ");
    ImGui::Text("\xee\x95\x8a""        ICON_FA_PERSON_MILITARY_POINTING               ");
    ImGui::Text("\xee\x95\x8b""        ICON_FA_PERSON_MILITARY_RIFLE                  ");
    ImGui::Text("\xee\x95\x8c""        ICON_FA_PERSON_MILITARY_TO_PERSON              ");
    ImGui::Text("\xef\x9a\x83""        ICON_FA_PERSON_PRAYING                         ");
    ImGui::Text("\xee\x8c\x9e""        ICON_FA_PERSON_PREGNANT                        ");
    ImGui::Text("\xee\x95\x8d""        ICON_FA_PERSON_RAYS                            ");
    ImGui::Text("\xee\x95\x8e""        ICON_FA_PERSON_RIFLE                           ");
    ImGui::Text("\xef\x9c\x8c""        ICON_FA_PERSON_RUNNING                         ");
    ImGui::Text("\xee\x95\x8f""        ICON_FA_PERSON_SHELTER                         ");
    ImGui::Text("\xef\x9f\x85""        ICON_FA_PERSON_SKATING                         ");
    ImGui::Text("\xef\x9f\x89""        ICON_FA_PERSON_SKIING                          ");
    ImGui::Text("\xef\x9f\x8a""        ICON_FA_PERSON_SKIING_NORDIC                   ");
    ImGui::Text("\xef\x9f\x8e""        ICON_FA_PERSON_SNOWBOARDING                    ");
    ImGui::Text("\xef\x97\x84""        ICON_FA_PERSON_SWIMMING                        ");
    ImGui::Text("\xee\x96\xa9""        ICON_FA_PERSON_THROUGH_WINDOW                  ");
    ImGui::Text("\xef\x95\x94""        ICON_FA_PERSON_WALKING                         ");
    ImGui::Text("\xee\x95\x91""        ICON_FA_PERSON_WALKING_ARROW_LOOP_LEFT         ");
    ImGui::Text("\xee\x95\x92""        ICON_FA_PERSON_WALKING_ARROW_RIGHT             ");
    ImGui::Text("\xee\x95\x93""        ICON_FA_PERSON_WALKING_DASHED_LINE_ARROW_RIGHT ");
    ImGui::Text("\xee\x95\x94""        ICON_FA_PERSON_WALKING_LUGGAGE                 ");
    ImGui::Text("\xef\x8a\x9d""        ICON_FA_PERSON_WALKING_WITH_CANE               ");
    ImGui::Text("\xee\x88\xa1""        ICON_FA_PESETA_SIGN                            ");
    ImGui::Text("\xee\x88\xa2""        ICON_FA_PESO_SIGN                              ");
    ImGui::Text("\xef\x82\x95""        ICON_FA_PHONE                                  ");
    ImGui::Text("\xef\xa1\xb9""        ICON_FA_PHONE_FLIP                             ");
    ImGui::Text("\xef\x8f\x9d""        ICON_FA_PHONE_SLASH                            ");
    ImGui::Text("\xef\x8a\xa0""        ICON_FA_PHONE_VOLUME                           ");
    ImGui::Text("\xef\xa1\xbc""        ICON_FA_PHOTO_FILM                             ");
    ImGui::Text("\xef\x93\x93""        ICON_FA_PIGGY_BANK                             ");
    ImGui::Text("\xef\x92\x84""        ICON_FA_PILLS                                  ");
    ImGui::Text("\xef\xa0\x98""        ICON_FA_PIZZA_SLICE                            ");
    ImGui::Text("\xef\x99\xbf""        ICON_FA_PLACE_OF_WORSHIP                       ");
    ImGui::Text("\xef\x81\xb2""        ICON_FA_PLANE                                  ");
    ImGui::Text("\xef\x96\xaf""        ICON_FA_PLANE_ARRIVAL                          ");
    ImGui::Text("\xee\x95\x95""        ICON_FA_PLANE_CIRCLE_CHECK                     ");
    ImGui::Text("\xee\x95\x96""        ICON_FA_PLANE_CIRCLE_EXCLAMATION               ");
    ImGui::Text("\xee\x95\x97""        ICON_FA_PLANE_CIRCLE_XMARK                     ");
    ImGui::Text("\xef\x96\xb0""        ICON_FA_PLANE_DEPARTURE                        ");
    ImGui::Text("\xee\x95\x98""        ICON_FA_PLANE_LOCK                             ");
    ImGui::Text("\xee\x81\xa9""        ICON_FA_PLANE_SLASH                            ");
    ImGui::Text("\xee\x88\xad""        ICON_FA_PLANE_UP                               ");
    ImGui::Text("\xee\x96\xaa""        ICON_FA_PLANT_WILT                             ");
    ImGui::Text("\xee\x95\x9a""        ICON_FA_PLATE_WHEAT                            ");
    ImGui::Text("\xef\x81\x8b""        ICON_FA_PLAY                                   ");
    ImGui::Text("\xef\x87\xa6""        ICON_FA_PLUG                                   ");
    ImGui::Text("\xee\x95\x9b""        ICON_FA_PLUG_CIRCLE_BOLT                       ");
    ImGui::Text("\xee\x95\x9c""        ICON_FA_PLUG_CIRCLE_CHECK                      ");
    ImGui::Text("\xee\x95\x9d""        ICON_FA_PLUG_CIRCLE_EXCLAMATION                ");
    ImGui::Text("\xee\x95\x9e""        ICON_FA_PLUG_CIRCLE_MINUS                      ");
    ImGui::Text("\xee\x95\x9f""        ICON_FA_PLUG_CIRCLE_PLUS                       ");
    ImGui::Text("\xee\x95\xa0""        ICON_FA_PLUG_CIRCLE_XMARK                      ");
    ImGui::Text("+"	       "        ICON_FA_PLUS                                   ");
    ImGui::Text("\xee\x90\xbc""        ICON_FA_PLUS_MINUS                             ");
    ImGui::Text("\xef\x8b\x8e""        ICON_FA_PODCAST                                ");
    ImGui::Text("\xef\x8b\xbe""        ICON_FA_POO                                    ");
    ImGui::Text("\xef\x9d\x9a""        ICON_FA_POO_STORM                              ");
    ImGui::Text("\xef\x98\x99""        ICON_FA_POOP                                   ");
    ImGui::Text("\xef\x80\x91""        ICON_FA_POWER_OFF                              ");
    ImGui::Text("\xef\x96\xb1""        ICON_FA_PRESCRIPTION                           ");
    ImGui::Text("\xef\x92\x85""        ICON_FA_PRESCRIPTION_BOTTLE                    ");
    ImGui::Text("\xef\x92\x86""        ICON_FA_PRESCRIPTION_BOTTLE_MEDICAL            ");
    ImGui::Text("\xef\x80\xaf""        ICON_FA_PRINT                                  ");
    ImGui::Text("\xee\x81\xaa""        ICON_FA_PUMP_MEDICAL                           ");
    ImGui::Text("\xee\x81\xab""        ICON_FA_PUMP_SOAP                              ");
    ImGui::Text("\xef\x84\xae""        ICON_FA_PUZZLE_PIECE                           ");
    ImGui::Text("Q"	       "        ICON_FA_Q                                      ");
    ImGui::Text("\xef\x80\xa9""        ICON_FA_QRCODE                                 ");
    ImGui::Text("?"	       "        ICON_FA_QUESTION                               ");
    ImGui::Text("\xef\x84\x8d""        ICON_FA_QUOTE_LEFT                             ");
    ImGui::Text("\xef\x84\x8e""        ICON_FA_QUOTE_RIGHT                            ");
    ImGui::Text("R"	       "        ICON_FA_R                                      ");
    ImGui::Text("\xef\x9e\xb9""        ICON_FA_RADIATION                              ");
    ImGui::Text("\xef\xa3\x97""        ICON_FA_RADIO                                  ");
    ImGui::Text("\xef\x9d\x9b""        ICON_FA_RAINBOW                                ");
    ImGui::Text("\xee\x95\xa1""        ICON_FA_RANKING_STAR                           ");
    ImGui::Text("\xef\x95\x83""        ICON_FA_RECEIPT                                ");
    ImGui::Text("\xef\xa3\x99""        ICON_FA_RECORD_VINYL                           ");
    ImGui::Text("\xef\x99\x81""        ICON_FA_RECTANGLE_AD                           ");
    ImGui::Text("\xef\x80\xa2""        ICON_FA_RECTANGLE_LIST                         ");
    ImGui::Text("\xef\x90\x90""        ICON_FA_RECTANGLE_XMARK                        ");
    ImGui::Text("\xef\x86\xb8""        ICON_FA_RECYCLE                                ");
    ImGui::Text("\xef\x89\x9d""        ICON_FA_REGISTERED                             ");
    ImGui::Text("\xef\x8d\xa3""        ICON_FA_REPEAT                                 ");
    ImGui::Text("\xef\x8f\xa5""        ICON_FA_REPLY                                  ");
    ImGui::Text("\xef\x84\xa2""        ICON_FA_REPLY_ALL                              ");
    ImGui::Text("\xef\x9d\x9e""        ICON_FA_REPUBLICAN                             ");
    ImGui::Text("\xef\x9e\xbd""        ICON_FA_RESTROOM                               ");
    ImGui::Text("\xef\x81\xb9""        ICON_FA_RETWEET                                ");
    ImGui::Text("\xef\x93\x96""        ICON_FA_RIBBON                                 ");
    ImGui::Text("\xef\x8b\xb5""        ICON_FA_RIGHT_FROM_BRACKET                     ");
    ImGui::Text("\xef\x8d\xa2""        ICON_FA_RIGHT_LEFT                             ");
    ImGui::Text("\xef\x8c\x8b""        ICON_FA_RIGHT_LONG                             ");
    ImGui::Text("\xef\x8b\xb6""        ICON_FA_RIGHT_TO_BRACKET                       ");
    ImGui::Text("\xef\x9c\x8b""        ICON_FA_RING                                   ");
    ImGui::Text("\xef\x80\x98""        ICON_FA_ROAD                                   ");
    ImGui::Text("\xee\x95\xa2""        ICON_FA_ROAD_BARRIER                           ");
    ImGui::Text("\xee\x95\xa3""        ICON_FA_ROAD_BRIDGE                            ");
    ImGui::Text("\xee\x95\xa4""        ICON_FA_ROAD_CIRCLE_CHECK                      ");
    ImGui::Text("\xee\x95\xa5""        ICON_FA_ROAD_CIRCLE_EXCLAMATION                ");
    ImGui::Text("\xee\x95\xa6""        ICON_FA_ROAD_CIRCLE_XMARK                      ");
    ImGui::Text("\xee\x95\xa7""        ICON_FA_ROAD_LOCK                              ");
    ImGui::Text("\xee\x95\xa8""        ICON_FA_ROAD_SPIKES                            ");
    ImGui::Text("\xef\x95\x84""        ICON_FA_ROBOT                                  ");
    ImGui::Text("\xef\x84\xb5""        ICON_FA_ROCKET                                 ");
    ImGui::Text("\xef\x8b\xb1""        ICON_FA_ROTATE                                 ");
    ImGui::Text("\xef\x8b\xaa""        ICON_FA_ROTATE_LEFT                            ");
    ImGui::Text("\xef\x8b\xb9""        ICON_FA_ROTATE_RIGHT                           ");
    ImGui::Text("\xef\x93\x97""        ICON_FA_ROUTE                                  ");
    ImGui::Text("\xef\x82\x9e""        ICON_FA_RSS                                    ");
    ImGui::Text("\xef\x85\x98""        ICON_FA_RUBLE_SIGN                             ");
    ImGui::Text("\xee\x95\xa9""        ICON_FA_RUG                                    ");
    ImGui::Text("\xef\x95\x85""        ICON_FA_RULER                                  ");
    ImGui::Text("\xef\x95\x86""        ICON_FA_RULER_COMBINED                         ");
    ImGui::Text("\xef\x95\x87""        ICON_FA_RULER_HORIZONTAL                       ");
    ImGui::Text("\xef\x95\x88""        ICON_FA_RULER_VERTICAL                         ");
    ImGui::Text("\xef\x85\x96""        ICON_FA_RUPEE_SIGN                             ");
    ImGui::Text("\xee\x88\xbd""        ICON_FA_RUPIAH_SIGN                            ");
    ImGui::Text("S"	       "        ICON_FA_S                                      ");
    ImGui::Text("\xef\xa0\x9d""        ICON_FA_SACK_DOLLAR                            ");
    ImGui::Text("\xee\x95\xaa""        ICON_FA_SACK_XMARK                             ");
    ImGui::Text("\xee\x91\x85""        ICON_FA_SAILBOAT                               ");
    ImGui::Text("\xef\x9e\xbf""        ICON_FA_SATELLITE                              ");
    ImGui::Text("\xef\x9f\x80""        ICON_FA_SATELLITE_DISH                         ");
    ImGui::Text("\xef\x89\x8e""        ICON_FA_SCALE_BALANCED                         ");
    ImGui::Text("\xef\x94\x95""        ICON_FA_SCALE_UNBALANCED                       ");
    ImGui::Text("\xef\x94\x96""        ICON_FA_SCALE_UNBALANCED_FLIP                  ");
    ImGui::Text("\xef\x95\x89""        ICON_FA_SCHOOL                                 ");
    ImGui::Text("\xee\x95\xab""        ICON_FA_SCHOOL_CIRCLE_CHECK                    ");
    ImGui::Text("\xee\x95\xac""        ICON_FA_SCHOOL_CIRCLE_EXCLAMATION              ");
    ImGui::Text("\xee\x95\xad""        ICON_FA_SCHOOL_CIRCLE_XMARK                    ");
    ImGui::Text("\xee\x95\xae""        ICON_FA_SCHOOL_FLAG                            ");
    ImGui::Text("\xee\x95\xaf""        ICON_FA_SCHOOL_LOCK                            ");
    ImGui::Text("\xef\x83\x84""        ICON_FA_SCISSORS                               ");
    ImGui::Text("\xef\x95\x8a""        ICON_FA_SCREWDRIVER                            ");
    ImGui::Text("\xef\x9f\x99""        ICON_FA_SCREWDRIVER_WRENCH                     ");
    ImGui::Text("\xef\x9c\x8e""        ICON_FA_SCROLL                                 ");
    ImGui::Text("\xef\x9a\xa0""        ICON_FA_SCROLL_TORAH                           ");
    ImGui::Text("\xef\x9f\x82""        ICON_FA_SD_CARD                                ");
    ImGui::Text("\xee\x91\x87""        ICON_FA_SECTION                                ");
    ImGui::Text("\xef\x93\x98""        ICON_FA_SEEDLING                               ");
    ImGui::Text("\xef\x88\xb3""        ICON_FA_SERVER                                 ");
    ImGui::Text("\xef\x98\x9f""        ICON_FA_SHAPES                                 ");
    ImGui::Text("\xef\x81\xa4""        ICON_FA_SHARE                                  ");
    ImGui::Text("\xef\x85\x8d""        ICON_FA_SHARE_FROM_SQUARE                      ");
    ImGui::Text("\xef\x87\xa0""        ICON_FA_SHARE_NODES                            ");
    ImGui::Text("\xee\x95\xb1""        ICON_FA_SHEET_PLASTIC                          ");
    ImGui::Text("\xef\x88\x8b""        ICON_FA_SHEKEL_SIGN                            ");
    ImGui::Text("\xef\x84\xb2""        ICON_FA_SHIELD                                 ");
    ImGui::Text("\xee\x95\xb2""        ICON_FA_SHIELD_CAT                             ");
    ImGui::Text("\xee\x95\xb3""        ICON_FA_SHIELD_DOG                             ");
    ImGui::Text("\xef\x8f\xad""        ICON_FA_SHIELD_HALVED                          ");
    ImGui::Text("\xee\x95\xb4""        ICON_FA_SHIELD_HEART                           ");
    ImGui::Text("\xee\x81\xac""        ICON_FA_SHIELD_VIRUS                           ");
    ImGui::Text("\xef\x88\x9a""        ICON_FA_SHIP                                   ");
    ImGui::Text("\xef\x95\x93""        ICON_FA_SHIRT                                  ");
    ImGui::Text("\xef\x95\x8b""        ICON_FA_SHOE_PRINTS                            ");
    ImGui::Text("\xef\x95\x8f""        ICON_FA_SHOP                                   ");
    ImGui::Text("\xee\x92\xa5""        ICON_FA_SHOP_LOCK                              ");
    ImGui::Text("\xee\x81\xb0""        ICON_FA_SHOP_SLASH                             ");
    ImGui::Text("\xef\x8b\x8c""        ICON_FA_SHOWER                                 ");
    ImGui::Text("\xee\x91\x88""        ICON_FA_SHRIMP                                 ");
    ImGui::Text("\xef\x81\xb4""        ICON_FA_SHUFFLE                                ");
    ImGui::Text("\xef\x86\x97""        ICON_FA_SHUTTLE_SPACE                          ");
    ImGui::Text("\xef\x93\x99""        ICON_FA_SIGN_HANGING                           ");
    ImGui::Text("\xef\x80\x92""        ICON_FA_SIGNAL                                 ");
    ImGui::Text("\xef\x96\xb7""        ICON_FA_SIGNATURE                              ");
    ImGui::Text("\xef\x89\xb7""        ICON_FA_SIGNS_POST                             ");
    ImGui::Text("\xef\x9f\x84""        ICON_FA_SIM_CARD                               ");
    ImGui::Text("\xee\x81\xad""        ICON_FA_SINK                                   ");
    ImGui::Text("\xef\x83\xa8""        ICON_FA_SITEMAP                                ");
    ImGui::Text("\xef\x95\x8c""        ICON_FA_SKULL                                  ");
    ImGui::Text("\xef\x9c\x94""        ICON_FA_SKULL_CROSSBONES                       ");
    ImGui::Text("\xef\x9c\x95""        ICON_FA_SLASH                                  ");
    ImGui::Text("\xef\x9f\x8c""        ICON_FA_SLEIGH                                 ");
    ImGui::Text("\xef\x87\x9e""        ICON_FA_SLIDERS                                ");
    ImGui::Text("\xef\x9d\x9f""        ICON_FA_SMOG                                   ");
    ImGui::Text("\xef\x92\x8d""        ICON_FA_SMOKING                                ");
    ImGui::Text("\xef\x8b\x9c""        ICON_FA_SNOWFLAKE                              ");
    ImGui::Text("\xef\x9f\x90""        ICON_FA_SNOWMAN                                ");
    ImGui::Text("\xef\x9f\x92""        ICON_FA_SNOWPLOW                               ");
    ImGui::Text("\xee\x81\xae""        ICON_FA_SOAP                                   ");
    ImGui::Text("\xef\x9a\x96""        ICON_FA_SOCKS                                  ");
    ImGui::Text("\xef\x96\xba""        ICON_FA_SOLAR_PANEL                            ");
    ImGui::Text("\xef\x83\x9c""        ICON_FA_SORT                                   ");
    ImGui::Text("\xef\x83\x9d""        ICON_FA_SORT_DOWN                              ");
    ImGui::Text("\xef\x83\x9e""        ICON_FA_SORT_UP                                ");
    ImGui::Text("\xef\x96\xbb""        ICON_FA_SPA                                    ");
    ImGui::Text("\xef\x99\xbb""        ICON_FA_SPAGHETTI_MONSTER_FLYING               ");
    ImGui::Text("\xef\xa2\x91""        ICON_FA_SPELL_CHECK                            ");
    ImGui::Text("\xef\x9c\x97""        ICON_FA_SPIDER                                 ");
    ImGui::Text("\xef\x84\x90""        ICON_FA_SPINNER                                ");
    ImGui::Text("\xef\x96\xbc""        ICON_FA_SPLOTCH                                ");
    ImGui::Text("\xef\x8b\xa5""        ICON_FA_SPOON                                  ");
    ImGui::Text("\xef\x96\xbd""        ICON_FA_SPRAY_CAN                              ");
    ImGui::Text("\xef\x97\x90""        ICON_FA_SPRAY_CAN_SPARKLES                     ");
    ImGui::Text("\xef\x83\x88""        ICON_FA_SQUARE                                 ");
    ImGui::Text("\xef\x85\x8c""        ICON_FA_SQUARE_ARROW_UP_RIGHT                  ");
    ImGui::Text("\xef\x85\x90""        ICON_FA_SQUARE_CARET_DOWN                      ");
    ImGui::Text("\xef\x86\x91""        ICON_FA_SQUARE_CARET_LEFT                      ");
    ImGui::Text("\xef\x85\x92""        ICON_FA_SQUARE_CARET_RIGHT                     ");
    ImGui::Text("\xef\x85\x91""        ICON_FA_SQUARE_CARET_UP                        ");
    ImGui::Text("\xef\x85\x8a""        ICON_FA_SQUARE_CHECK                           ");
    ImGui::Text("\xef\x86\x99""        ICON_FA_SQUARE_ENVELOPE                        ");
    ImGui::Text("\xef\x91\x9c""        ICON_FA_SQUARE_FULL                            ");
    ImGui::Text("\xef\x83\xbd""        ICON_FA_SQUARE_H                               ");
    ImGui::Text("\xef\x85\x86""        ICON_FA_SQUARE_MINUS                           ");
    ImGui::Text("\xee\x95\xb6""        ICON_FA_SQUARE_NFI                             ");
    ImGui::Text("\xef\x95\x80""        ICON_FA_SQUARE_PARKING                         ");
    ImGui::Text("\xef\x85\x8b""        ICON_FA_SQUARE_PEN                             ");
    ImGui::Text("\xee\x95\xb7""        ICON_FA_SQUARE_PERSON_CONFINED                 ");
    ImGui::Text("\xef\x82\x98""        ICON_FA_SQUARE_PHONE                           ");
    ImGui::Text("\xef\xa1\xbb""        ICON_FA_SQUARE_PHONE_FLIP                      ");
    ImGui::Text("\xef\x83\xbe""        ICON_FA_SQUARE_PLUS                            ");
    ImGui::Text("\xef\x9a\x82""        ICON_FA_SQUARE_POLL_HORIZONTAL                 ");
    ImGui::Text("\xef\x9a\x81""        ICON_FA_SQUARE_POLL_VERTICAL                   ");
    ImGui::Text("\xef\x9a\x98""        ICON_FA_SQUARE_ROOT_VARIABLE                   ");
    ImGui::Text("\xef\x85\x83""        ICON_FA_SQUARE_RSS                             ");
    ImGui::Text("\xef\x87\xa1""        ICON_FA_SQUARE_SHARE_NODES                     ");
    ImGui::Text("\xef\x8d\xa0""        ICON_FA_SQUARE_UP_RIGHT                        ");
    ImGui::Text("\xee\x95\xb8""        ICON_FA_SQUARE_VIRUS                           ");
    ImGui::Text("\xef\x8b\x93""        ICON_FA_SQUARE_XMARK                           ");
    ImGui::Text("\xee\x95\xb9""        ICON_FA_STAFF_SNAKE                            ");
    ImGui::Text("\xee\x8a\x89""        ICON_FA_STAIRS                                 ");
    ImGui::Text("\xef\x96\xbf""        ICON_FA_STAMP                                  ");
    ImGui::Text("\xee\x96\xaf""        ICON_FA_STAPLER                                ");
    ImGui::Text("\xef\x80\x85""        ICON_FA_STAR                                   ");
    ImGui::Text("\xef\x9a\x99""        ICON_FA_STAR_AND_CRESCENT                      ");
    ImGui::Text("\xef\x82\x89""        ICON_FA_STAR_HALF                              ");
    ImGui::Text("\xef\x97\x80""        ICON_FA_STAR_HALF_STROKE                       ");
    ImGui::Text("\xef\x9a\x9a""        ICON_FA_STAR_OF_DAVID                          ");
    ImGui::Text("\xef\x98\xa1""        ICON_FA_STAR_OF_LIFE                           ");
    ImGui::Text("\xef\x85\x94""        ICON_FA_STERLING_SIGN                          ");
    ImGui::Text("\xef\x83\xb1""        ICON_FA_STETHOSCOPE                            ");
    ImGui::Text("\xef\x81\x8d""        ICON_FA_STOP                                   ");
    ImGui::Text("\xef\x8b\xb2""        ICON_FA_STOPWATCH                              ");
    ImGui::Text("\xee\x81\xaf""        ICON_FA_STOPWATCH_20                           ");
    ImGui::Text("\xef\x95\x8e""        ICON_FA_STORE                                  ");
    ImGui::Text("\xee\x81\xb1""        ICON_FA_STORE_SLASH                            ");
    ImGui::Text("\xef\x88\x9d""        ICON_FA_STREET_VIEW                            ");
    ImGui::Text("\xef\x83\x8c""        ICON_FA_STRIKETHROUGH                          ");
    ImGui::Text("\xef\x95\x91""        ICON_FA_STROOPWAFEL                            ");
    ImGui::Text("\xef\x84\xac""        ICON_FA_SUBSCRIPT                              ");
    ImGui::Text("\xef\x83\xb2""        ICON_FA_SUITCASE                               ");
    ImGui::Text("\xef\x83\xba""        ICON_FA_SUITCASE_MEDICAL                       ");
    ImGui::Text("\xef\x97\x81""        ICON_FA_SUITCASE_ROLLING                       ");
    ImGui::Text("\xef\x86\x85""        ICON_FA_SUN                                    ");
    ImGui::Text("\xee\x95\xba""        ICON_FA_SUN_PLANT_WILT                         ");
    ImGui::Text("\xef\x84\xab""        ICON_FA_SUPERSCRIPT                            ");
    ImGui::Text("\xef\x97\x83""        ICON_FA_SWATCHBOOK                             ");
    ImGui::Text("\xef\x9a\x9b""        ICON_FA_SYNAGOGUE                              ");
    ImGui::Text("\xef\x92\x8e""        ICON_FA_SYRINGE                                ");
    ImGui::Text("T"	       "        ICON_FA_T                                      ");
    ImGui::Text("\xef\x83\x8e""        ICON_FA_TABLE                                  ");
    ImGui::Text("\xef\x80\x8a""        ICON_FA_TABLE_CELLS                            ");
    ImGui::Text("\xef\x80\x89""        ICON_FA_TABLE_CELLS_LARGE                      ");
    ImGui::Text("\xef\x83\x9b""        ICON_FA_TABLE_COLUMNS                          ");
    ImGui::Text("\xef\x80\x8b""        ICON_FA_TABLE_LIST                             ");
    ImGui::Text("\xef\x91\x9d""        ICON_FA_TABLE_TENNIS_PADDLE_BALL               ");
    ImGui::Text("\xef\x8f\xbb""        ICON_FA_TABLET                                 ");
    ImGui::Text("\xef\x84\x8a""        ICON_FA_TABLET_BUTTON                          ");
    ImGui::Text("\xef\x8f\xba""        ICON_FA_TABLET_SCREEN_BUTTON                   ");
    ImGui::Text("\xef\x92\x90""        ICON_FA_TABLETS                                ");
    ImGui::Text("\xef\x95\xa6""        ICON_FA_TACHOGRAPH_DIGITAL                     ");
    ImGui::Text("\xef\x80\xab""        ICON_FA_TAG                                    ");
    ImGui::Text("\xef\x80\xac""        ICON_FA_TAGS                                   ");
    ImGui::Text("\xef\x93\x9b""        ICON_FA_TAPE                                   ");
    ImGui::Text("\xee\x95\xbb""        ICON_FA_TARP                                   ");
    ImGui::Text("\xee\x95\xbc""        ICON_FA_TARP_DROPLET                           ");
    ImGui::Text("\xef\x86\xba""        ICON_FA_TAXI                                   ");
    ImGui::Text("\xef\x98\xae""        ICON_FA_TEETH                                  ");
    ImGui::Text("\xef\x98\xaf""        ICON_FA_TEETH_OPEN                             ");
    ImGui::Text("\xee\x80\xbf""        ICON_FA_TEMPERATURE_ARROW_DOWN                 ");
    ImGui::Text("\xee\x81\x80""        ICON_FA_TEMPERATURE_ARROW_UP                   ");
    ImGui::Text("\xef\x8b\x8b""        ICON_FA_TEMPERATURE_EMPTY                      ");
    ImGui::Text("\xef\x8b\x87""        ICON_FA_TEMPERATURE_FULL                       ");
    ImGui::Text("\xef\x8b\x89""        ICON_FA_TEMPERATURE_HALF                       ");
    ImGui::Text("\xef\x9d\xa9""        ICON_FA_TEMPERATURE_HIGH                       ");
    ImGui::Text("\xef\x9d\xab""        ICON_FA_TEMPERATURE_LOW                        ");
    ImGui::Text("\xef\x8b\x8a""        ICON_FA_TEMPERATURE_QUARTER                    ");
    ImGui::Text("\xef\x8b\x88""        ICON_FA_TEMPERATURE_THREE_QUARTERS             ");
    ImGui::Text("\xef\x9f\x97""        ICON_FA_TENGE_SIGN                             ");
    ImGui::Text("\xee\x95\xbd""        ICON_FA_TENT                                   ");
    ImGui::Text("\xee\x95\xbe""        ICON_FA_TENT_ARROW_DOWN_TO_LINE                ");
    ImGui::Text("\xee\x95\xbf""        ICON_FA_TENT_ARROW_LEFT_RIGHT                  ");
    ImGui::Text("\xee\x96\x80""        ICON_FA_TENT_ARROW_TURN_LEFT                   ");
    ImGui::Text("\xee\x96\x81""        ICON_FA_TENT_ARROWS_DOWN                       ");
    ImGui::Text("\xee\x96\x82""        ICON_FA_TENTS                                  ");
    ImGui::Text("\xef\x84\xa0""        ICON_FA_TERMINAL                               ");
    ImGui::Text("\xef\x80\xb4""        ICON_FA_TEXT_HEIGHT                            ");
    ImGui::Text("\xef\xa1\xbd""        ICON_FA_TEXT_SLASH                             ");
    ImGui::Text("\xef\x80\xb5""        ICON_FA_TEXT_WIDTH                             ");
    ImGui::Text("\xef\x92\x91""        ICON_FA_THERMOMETER                            ");
    ImGui::Text("\xef\x85\xa5""        ICON_FA_THUMBS_DOWN                            ");
    ImGui::Text("\xef\x85\xa4""        ICON_FA_THUMBS_UP                              ");
    ImGui::Text("\xef\x82\x8d""        ICON_FA_THUMBTACK                              ");
    ImGui::Text("\xef\x85\x85""        ICON_FA_TICKET                                 ");
    ImGui::Text("\xef\x8f\xbf""        ICON_FA_TICKET_SIMPLE                          ");
    ImGui::Text("\xee\x8a\x9c""        ICON_FA_TIMELINE                               ");
    ImGui::Text("\xef\x88\x84""        ICON_FA_TOGGLE_OFF                             ");
    ImGui::Text("\xef\x88\x85""        ICON_FA_TOGGLE_ON                              ");
    ImGui::Text("\xef\x9f\x98""        ICON_FA_TOILET                                 ");
    ImGui::Text("\xef\x9c\x9e""        ICON_FA_TOILET_PAPER                           ");
    ImGui::Text("\xee\x81\xb2""        ICON_FA_TOILET_PAPER_SLASH                     ");
    ImGui::Text("\xee\x96\x83""        ICON_FA_TOILET_PORTABLE                        ");
    ImGui::Text("\xee\x96\x84""        ICON_FA_TOILETS_PORTABLE                       ");
    ImGui::Text("\xef\x95\x92""        ICON_FA_TOOLBOX                                ");
    ImGui::Text("\xef\x97\x89""        ICON_FA_TOOTH                                  ");
    ImGui::Text("\xef\x9a\xa1""        ICON_FA_TORII_GATE                             ");
    ImGui::Text("\xef\x9d\xaf""        ICON_FA_TORNADO                                ");
    ImGui::Text("\xef\x94\x99""        ICON_FA_TOWER_BROADCAST                        ");
    ImGui::Text("\xee\x96\x85""        ICON_FA_TOWER_CELL                             ");
    ImGui::Text("\xee\x96\x86""        ICON_FA_TOWER_OBSERVATION                      ");
    ImGui::Text("\xef\x9c\xa2""        ICON_FA_TRACTOR                                ");
    ImGui::Text("\xef\x89\x9c""        ICON_FA_TRADEMARK                              ");
    ImGui::Text("\xef\x98\xb7""        ICON_FA_TRAFFIC_LIGHT                          ");
    ImGui::Text("\xee\x81\x81""        ICON_FA_TRAILER                                ");
    ImGui::Text("\xef\x88\xb8""        ICON_FA_TRAIN                                  ");
    ImGui::Text("\xef\x88\xb9""        ICON_FA_TRAIN_SUBWAY                           ");
    ImGui::Text("\xee\x96\xb4""        ICON_FA_TRAIN_TRAM                             ");
    ImGui::Text("\xef\x88\xa5""        ICON_FA_TRANSGENDER                            ");
    ImGui::Text("\xef\x87\xb8""        ICON_FA_TRASH                                  ");
    ImGui::Text("\xef\xa0\xa9""        ICON_FA_TRASH_ARROW_UP                         ");
    ImGui::Text("\xef\x8b\xad""        ICON_FA_TRASH_CAN                              ");
    ImGui::Text("\xef\xa0\xaa""        ICON_FA_TRASH_CAN_ARROW_UP                     ");
    ImGui::Text("\xef\x86\xbb""        ICON_FA_TREE                                   ");
    ImGui::Text("\xee\x96\x87""        ICON_FA_TREE_CITY                              ");
    ImGui::Text("\xef\x81\xb1""        ICON_FA_TRIANGLE_EXCLAMATION                   ");
    ImGui::Text("\xef\x82\x91""        ICON_FA_TROPHY                                 ");
    ImGui::Text("\xee\x96\x89""        ICON_FA_TROWEL                                 ");
    ImGui::Text("\xee\x96\x8a""        ICON_FA_TROWEL_BRICKS                          ");
    ImGui::Text("\xef\x83\x91""        ICON_FA_TRUCK                                  ");
    ImGui::Text("\xee\x96\x8b""        ICON_FA_TRUCK_ARROW_RIGHT                      ");
    ImGui::Text("\xee\x96\x8c""        ICON_FA_TRUCK_DROPLET                          ");
    ImGui::Text("\xef\x92\x8b""        ICON_FA_TRUCK_FAST                             ");
    ImGui::Text("\xee\x96\x8d""        ICON_FA_TRUCK_FIELD                            ");
    ImGui::Text("\xee\x96\x8e""        ICON_FA_TRUCK_FIELD_UN                         ");
    ImGui::Text("\xee\x8a\xb7""        ICON_FA_TRUCK_FRONT                            ");
    ImGui::Text("\xef\x83\xb9""        ICON_FA_TRUCK_MEDICAL                          ");
    ImGui::Text("\xef\x98\xbb""        ICON_FA_TRUCK_MONSTER                          ");
    ImGui::Text("\xef\x93\x9f""        ICON_FA_TRUCK_MOVING                           ");
    ImGui::Text("\xef\x98\xbc""        ICON_FA_TRUCK_PICKUP                           ");
    ImGui::Text("\xee\x96\x8f""        ICON_FA_TRUCK_PLANE                            ");
    ImGui::Text("\xef\x93\x9e""        ICON_FA_TRUCK_RAMP_BOX                         ");
    ImGui::Text("\xef\x87\xa4""        ICON_FA_TTY                                    ");
    ImGui::Text("\xee\x8a\xbb""        ICON_FA_TURKISH_LIRA_SIGN                      ");
    ImGui::Text("\xef\x8e\xbe""        ICON_FA_TURN_DOWN                              ");
    ImGui::Text("\xef\x8e\xbf""        ICON_FA_TURN_UP                                ");
    ImGui::Text("\xef\x89\xac""        ICON_FA_TV                                     ");
    ImGui::Text("U"	       "        ICON_FA_U                                      ");
    ImGui::Text("\xef\x83\xa9""        ICON_FA_UMBRELLA                               ");
    ImGui::Text("\xef\x97\x8a""        ICON_FA_UMBRELLA_BEACH                         ");
    ImGui::Text("\xef\x83\x8d""        ICON_FA_UNDERLINE                              ");
    ImGui::Text("\xef\x8a\x9a""        ICON_FA_UNIVERSAL_ACCESS                       ");
    ImGui::Text("\xef\x82\x9c""        ICON_FA_UNLOCK                                 ");
    ImGui::Text("\xef\x84\xbe""        ICON_FA_UNLOCK_KEYHOLE                         ");
    ImGui::Text("\xef\x8c\xb8""        ICON_FA_UP_DOWN                                ");
    ImGui::Text("\xef\x82\xb2""        ICON_FA_UP_DOWN_LEFT_RIGHT                     ");
    ImGui::Text("\xef\x8c\x8c""        ICON_FA_UP_LONG                                ");
    ImGui::Text("\xef\x90\xa4""        ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER     ");
    ImGui::Text("\xef\x8d\x9d""        ICON_FA_UP_RIGHT_FROM_SQUARE                   ");
    ImGui::Text("\xef\x82\x93""        ICON_FA_UPLOAD                                 ");
    ImGui::Text("\xef\x80\x87""        ICON_FA_USER                                   ");
    ImGui::Text("\xef\x93\xbb""        ICON_FA_USER_ASTRONAUT                         ");
    ImGui::Text("\xef\x93\xbc""        ICON_FA_USER_CHECK                             ");
    ImGui::Text("\xef\x93\xbd""        ICON_FA_USER_CLOCK                             ");
    ImGui::Text("\xef\x83\xb0""        ICON_FA_USER_DOCTOR                            ");
    ImGui::Text("\xef\x93\xbe""        ICON_FA_USER_GEAR                              ");
    ImGui::Text("\xef\x94\x81""        ICON_FA_USER_GRADUATE                          ");
    ImGui::Text("\xef\x94\x80""        ICON_FA_USER_GROUP                             ");
    ImGui::Text("\xef\x9c\xa8""        ICON_FA_USER_INJURED                           ");
    ImGui::Text("\xef\x90\x86""        ICON_FA_USER_LARGE                             ");
    ImGui::Text("\xef\x93\xba""        ICON_FA_USER_LARGE_SLASH                       ");
    ImGui::Text("\xef\x94\x82""        ICON_FA_USER_LOCK                              ");
    ImGui::Text("\xef\x94\x83""        ICON_FA_USER_MINUS                             ");
    ImGui::Text("\xef\x94\x84""        ICON_FA_USER_NINJA                             ");
    ImGui::Text("\xef\xa0\xaf""        ICON_FA_USER_NURSE                             ");
    ImGui::Text("\xef\x93\xbf""        ICON_FA_USER_PEN                               ");
    ImGui::Text("\xef\x88\xb4""        ICON_FA_USER_PLUS                              ");
    ImGui::Text("\xef\x88\x9b""        ICON_FA_USER_SECRET                            ");
    ImGui::Text("\xef\x94\x85""        ICON_FA_USER_SHIELD                            ");
    ImGui::Text("\xef\x94\x86""        ICON_FA_USER_SLASH                             ");
    ImGui::Text("\xef\x94\x87""        ICON_FA_USER_TAG                               ");
    ImGui::Text("\xef\x94\x88""        ICON_FA_USER_TIE                               ");
    ImGui::Text("\xef\x88\xb5""        ICON_FA_USER_XMARK                             ");
    ImGui::Text("\xef\x83\x80""        ICON_FA_USERS                                  ");
    ImGui::Text("\xee\x96\x91""        ICON_FA_USERS_BETWEEN_LINES                    ");
    ImGui::Text("\xef\x94\x89""        ICON_FA_USERS_GEAR                             ");
    ImGui::Text("\xee\x96\x92""        ICON_FA_USERS_LINE                             ");
    ImGui::Text("\xee\x96\x93""        ICON_FA_USERS_RAYS                             ");
    ImGui::Text("\xee\x96\x94""        ICON_FA_USERS_RECTANGLE                        ");
    ImGui::Text("\xee\x81\xb3""        ICON_FA_USERS_SLASH                            ");
    ImGui::Text("\xee\x96\x95""        ICON_FA_USERS_VIEWFINDER                       ");
    ImGui::Text("\xef\x8b\xa7""        ICON_FA_UTENSILS                               ");
    ImGui::Text("V"	       "        ICON_FA_V                                      ");
    ImGui::Text("\xef\x96\xb6""        ICON_FA_VAN_SHUTTLE                            ");
    ImGui::Text("\xee\x8b\x85""        ICON_FA_VAULT                                  ");
    ImGui::Text("\xef\x97\x8b""        ICON_FA_VECTOR_SQUARE                          ");
    ImGui::Text("\xef\x88\xa1""        ICON_FA_VENUS                                  ");
    ImGui::Text("\xef\x88\xa6""        ICON_FA_VENUS_DOUBLE                           ");
    ImGui::Text("\xef\x88\xa8""        ICON_FA_VENUS_MARS                             ");
    ImGui::Text("\xee\x82\x85""        ICON_FA_VEST                                   ");
    ImGui::Text("\xee\x82\x86""        ICON_FA_VEST_PATCHES                           ");
    ImGui::Text("\xef\x92\x92""        ICON_FA_VIAL                                   ");
    ImGui::Text("\xee\x96\x96""        ICON_FA_VIAL_CIRCLE_CHECK                      ");
    ImGui::Text("\xee\x96\x97""        ICON_FA_VIAL_VIRUS                             ");
    ImGui::Text("\xef\x92\x93""        ICON_FA_VIALS                                  ");
    ImGui::Text("\xef\x80\xbd""        ICON_FA_VIDEO                                  ");
    ImGui::Text("\xef\x93\xa2""        ICON_FA_VIDEO_SLASH                            ");
    ImGui::Text("\xef\x9a\xa7""        ICON_FA_VIHARA                                 ");
    ImGui::Text("\xee\x81\xb4""        ICON_FA_VIRUS                                  ");
    ImGui::Text("\xee\x92\xa8""        ICON_FA_VIRUS_COVID                            ");
    ImGui::Text("\xee\x92\xa9""        ICON_FA_VIRUS_COVID_SLASH                      ");
    ImGui::Text("\xee\x81\xb5""        ICON_FA_VIRUS_SLASH                            ");
    ImGui::Text("\xee\x81\xb6""        ICON_FA_VIRUSES                                ");
    ImGui::Text("\xef\xa2\x97""        ICON_FA_VOICEMAIL                              ");
    ImGui::Text("\xef\x9d\xb0""        ICON_FA_VOLCANO                                ");
    ImGui::Text("\xef\x91\x9f""        ICON_FA_VOLLEYBALL                             ");
    ImGui::Text("\xef\x80\xa8""        ICON_FA_VOLUME_HIGH                            ");
    ImGui::Text("\xef\x80\xa7""        ICON_FA_VOLUME_LOW                             ");
    ImGui::Text("\xef\x80\xa6""        ICON_FA_VOLUME_OFF                             ");
    ImGui::Text("\xef\x9a\xa9""        ICON_FA_VOLUME_XMARK                           ");
    ImGui::Text("\xef\x9c\xa9""        ICON_FA_VR_CARDBOARD                           ");
    ImGui::Text("W"	       "        ICON_FA_W                                      ");
    ImGui::Text("\xef\xa3\xaf""        ICON_FA_WALKIE_TALKIE                          ");
    ImGui::Text("\xef\x95\x95""        ICON_FA_WALLET                                 ");
    ImGui::Text("\xef\x83\x90""        ICON_FA_WAND_MAGIC                             ");
    ImGui::Text("\xee\x8b\x8a""        ICON_FA_WAND_MAGIC_SPARKLES                    ");
    ImGui::Text("\xef\x9c\xab""        ICON_FA_WAND_SPARKLES                          ");
    ImGui::Text("\xef\x92\x94""        ICON_FA_WAREHOUSE                              ");
    ImGui::Text("\xef\x9d\xb3""        ICON_FA_WATER                                  ");
    ImGui::Text("\xef\x97\x85""        ICON_FA_WATER_LADDER                           ");
    ImGui::Text("\xef\xa0\xbe""        ICON_FA_WAVE_SQUARE                            ");
    ImGui::Text("\xef\x97\x8d""        ICON_FA_WEIGHT_HANGING                         ");
    ImGui::Text("\xef\x92\x96""        ICON_FA_WEIGHT_SCALE                           ");
    ImGui::Text("\xee\x8b\x8d""        ICON_FA_WHEAT_AWN                              ");
    ImGui::Text("\xee\x96\x98""        ICON_FA_WHEAT_AWN_CIRCLE_EXCLAMATION           ");
    ImGui::Text("\xef\x86\x93""        ICON_FA_WHEELCHAIR                             ");
    ImGui::Text("\xee\x8b\x8e""        ICON_FA_WHEELCHAIR_MOVE                        ");
    ImGui::Text("\xef\x9e\xa0""        ICON_FA_WHISKEY_GLASS                          ");
    ImGui::Text("\xef\x87\xab""        ICON_FA_WIFI                                   ");
    ImGui::Text("\xef\x9c\xae""        ICON_FA_WIND                                   ");
    ImGui::Text("\xef\x8b\x90""        ICON_FA_WINDOW_MAXIMIZE                        ");
    ImGui::Text("\xef\x8b\x91""        ICON_FA_WINDOW_MINIMIZE                        ");
    ImGui::Text("\xef\x8b\x92""        ICON_FA_WINDOW_RESTORE                         ");
    ImGui::Text("\xef\x9c\xaf""        ICON_FA_WINE_BOTTLE                            ");
    ImGui::Text("\xef\x93\xa3""        ICON_FA_WINE_GLASS                             ");
    ImGui::Text("\xef\x97\x8e""        ICON_FA_WINE_GLASS_EMPTY                       ");
    ImGui::Text("\xef\x85\x99""        ICON_FA_WON_SIGN                               ");
    ImGui::Text("\xee\x96\x99""        ICON_FA_WORM                                   ");
    ImGui::Text("\xef\x82\xad""        ICON_FA_WRENCH                                 ");
    ImGui::Text("X"	       "        ICON_FA_X                                      ");
    ImGui::Text("\xef\x92\x97""        ICON_FA_X_RAY                                  ");
    ImGui::Text("\xef\x80\x8d""        ICON_FA_XMARK                                  ");
    ImGui::Text("\xee\x96\x9a""        ICON_FA_XMARKS_LINES                           ");
    ImGui::Text("Y"	       "        ICON_FA_Y                                      ");
    ImGui::Text("\xef\x85\x97""        ICON_FA_YEN_SIGN                               ");
    ImGui::Text("\xef\x9a\xad""        ICON_FA_YIN_YANG                               ");
    ImGui::Text("Z"	       "        ICON_FA_Z                                      ");

}

void MainWindow::ShowPageASerialScanner()
{
    ImGui::Text("扫描功能", FirstIdx, SecondIdx);
    //选项卡示例
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("单相机"))
        {
            ImGui::Text("单相机扫描例程");
            if (ImGui::SmallButton("开始扫描")) {
                ImGui::Text("开始单相机扫描");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void MainWindow::release()
{
    google::RemoveLogSink(&log_sink);
    google::ShutdownGoogleLogging();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

// 扫描器操作函数实现
void MainWindow::ScannerInitAsync(const std::string& config_path) {
    if (scanner_status_ != ScannerStatus::IDLE && scanner_status_ != ScannerStatus::ERROR_SCANNER) {
        LOG(WARNING) << "扫描器正在运行，无法初始化";
        return;
    }
    
    scanner_status_ = ScannerStatus::INITIALIZING;
    scanner_config_path_ = config_path;
    
    std::thread init_thread([this, config_path]() {
        try {
            LOG(INFO) << "开始初始化扫描器，配置路径: " << config_path;
            std::lock_guard<std::mutex> lock(scanner_mutex_);
            
            if (!scanner_api_) {
                scanner_api_ = std::make_unique<ScannerLApi>();
            }
            
            scanner_api_->SetConfigRootPath(config_path);
            int result = scanner_api_->Init();
            
            if (result == 0) {
                scanner_status_ = ScannerStatus::IDLE;
                LOG(INFO) << "扫描器初始化成功";
            } else {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器初始化失败，错误码: " << result;
            }
        } catch (const std::exception& e) {
            scanner_status_ = ScannerStatus::ERROR_SCANNER;
            LOG(ERROR) << "扫描器初始化异常: " << e.what();
        }
    });
    
    scanner_threads_.push_back(std::move(init_thread));
    CleanupScannerThreads(); // 清理已完成的线程
}

void MainWindow::ScannerConnectAsync() {
    if (scanner_status_ != ScannerStatus::IDLE) {
        LOG(WARNING) << "扫描器状态不正确，无法连接。当前状态: " << GetScannerStatusString();
        return;
    }
    
    scanner_status_ = ScannerStatus::CONNECTING;
    
    std::thread connect_thread([this]() {
        try {
            LOG(INFO) << "开始连接扫描器";
            std::lock_guard<std::mutex> lock(scanner_mutex_);
            
            if (!scanner_api_) {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器未初始化";
                return;
            }
            
            int result = scanner_api_->Connect();
            
            if (result == 0) {
                scanner_status_ = ScannerStatus::CONNECTED;
                LOG(INFO) << "扫描器连接成功";
            } else {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器连接失败，错误码: " << result;
            }
        } catch (const std::exception& e) {
            scanner_status_ = ScannerStatus::ERROR_SCANNER;
            LOG(ERROR) << "扫描器连接异常: " << e.what();
        }
    });
    
    scanner_threads_.push_back(std::move(connect_thread));
    CleanupScannerThreads();
}

void MainWindow::ScannerResetAsync() {
    if (scanner_status_ != ScannerStatus::CONNECTED) {
        LOG(WARNING) << "扫描器未连接，无法复位";
        return;
    }
    
    scanner_status_ = ScannerStatus::RESETTING;
    
    std::thread reset_thread([this]() {
        try {
            LOG(INFO) << "开始复位扫描器";
            std::lock_guard<std::mutex> lock(scanner_mutex_);
            
            if (!scanner_api_) {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器未初始化";
                return;
            }
            
            int result = scanner_api_->Reset();
            
            if (result == 0) {
                scanner_status_ = ScannerStatus::CONNECTED;
                LOG(INFO) << "扫描器复位成功";
            } else {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器复位失败，错误码: " << result;
            }
        } catch (const std::exception& e) {
            scanner_status_ = ScannerStatus::ERROR_SCANNER;
            LOG(ERROR) << "扫描器复位异常: " << e.what();
        }
    });
    
    scanner_threads_.push_back(std::move(reset_thread));
    CleanupScannerThreads();
}

void MainWindow::ScannerStartAsync() {
    if (scanner_status_ != ScannerStatus::CONNECTED) {
        LOG(WARNING) << "扫描器未连接，无法启动";
        return;
    }
    
    scanner_status_ = ScannerStatus::STARTING;
    
    std::thread start_thread([this]() {
        try {
            LOG(INFO) << "开始启动扫描";
            std::lock_guard<std::mutex> lock(scanner_mutex_);
            
            if (!scanner_api_) {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器未初始化";
                return;
            }
            
            int result = scanner_api_->Start();
            
            if (result == 0) {
                scanner_status_ = ScannerStatus::SCANNING;
                LOG(INFO) << "扫描启动成功";
            } else {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描启动失败，错误码: " << result;
            }
        } catch (const std::exception& e) {
            scanner_status_ = ScannerStatus::ERROR_SCANNER;
            LOG(ERROR) << "扫描启动异常: " << e.what();
        }
    });
    
    scanner_threads_.push_back(std::move(start_thread));
    CleanupScannerThreads();
}

void MainWindow::ScannerEndAsync() {
    if (scanner_status_ != ScannerStatus::SCANNING) {
        LOG(WARNING) << "扫描器未在扫描，无法停止";
        return;
    }
    
    scanner_status_ = ScannerStatus::STOPPING;
    
    std::thread end_thread([this]() {
        try {
            LOG(INFO) << "开始停止扫描";
            std::lock_guard<std::mutex> lock(scanner_mutex_);
            
            if (!scanner_api_) {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器未初始化";
                return;
            }
            
            int result = scanner_api_->End();
            
            if (result == 0) {
                scanner_status_ = ScannerStatus::CONNECTED;
                LOG(INFO) << "扫描停止成功";
            } else {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描停止失败，错误码: " << result;
            }
        } catch (const std::exception& e) {
            scanner_status_ = ScannerStatus::ERROR_SCANNER;
            LOG(ERROR) << "扫描停止异常: " << e.what();
        }
    });
    
    scanner_threads_.push_back(std::move(end_thread));
    CleanupScannerThreads();
}

void MainWindow::ScannerDisconnectAsync() {
    if (scanner_status_ == ScannerStatus::IDLE || scanner_status_ == ScannerStatus::DISCONNECTING) {
        return;
    }
    
    scanner_status_ = ScannerStatus::DISCONNECTING;
    
    std::thread disconnect_thread([this]() {
        try {
            LOG(INFO) << "开始断开扫描器连接";
            std::lock_guard<std::mutex> lock(scanner_mutex_);
            
            if (!scanner_api_) {
                scanner_status_ = ScannerStatus::IDLE;
                return;
            }
            
            int result = scanner_api_->disconnect();
            
            if (result == 0) {
                scanner_status_ = ScannerStatus::IDLE;
                LOG(INFO) << "扫描器断开连接成功";
            } else {
                scanner_status_ = ScannerStatus::ERROR_SCANNER;
                LOG(ERROR) << "扫描器断开连接失败，错误码: " << result;
            }
        } catch (const std::exception& e) {
            scanner_status_ = ScannerStatus::ERROR_SCANNER;
            LOG(ERROR) << "扫描器断开连接异常: " << e.what();
        }
    });
    
    scanner_threads_.push_back(std::move(disconnect_thread));
    CleanupScannerThreads();
}

ScannerStatus MainWindow::GetScannerStatus() const {
    return scanner_status_.load();
}

std::string MainWindow::GetScannerStatusString() const {
    switch (scanner_status_.load()) {
        case ScannerStatus::IDLE: return "空闲";
        case ScannerStatus::INITIALIZING: return "初始化中...";
        case ScannerStatus::CONNECTING: return "连接中...";
        case ScannerStatus::CONNECTED: return "已连接";
        case ScannerStatus::RESETTING: return "复位中...";
        case ScannerStatus::STARTING: return "启动中...";
        case ScannerStatus::SCANNING: return "扫描中...";
        case ScannerStatus::STOPPING: return "停止中...";
        case ScannerStatus::DISCONNECTING: return "断开连接中...";
        case ScannerStatus::ERROR_SCANNER: return "错误";
        default: return "未知";
    }
}

void MainWindow::CleanupScannerThreads() {
    // 限制线程数量，避免过多线程堆积
    // 注意：这里不主动join，让线程自然完成并detach
    // 实际项目中可以考虑使用线程池或更复杂的线程管理
    if (scanner_threads_.size() > 20) {
        // 如果线程太多，等待最老的几个线程完成
        for (size_t i = 0; i < 5 && i < scanner_threads_.size(); ++i) {
            if (scanner_threads_[i].joinable()) {
                scanner_threads_[i].detach(); // detach而不是join，避免阻塞
            }
        }
        scanner_threads_.erase(scanner_threads_.begin(), scanner_threads_.begin() + std::min(5, (int)scanner_threads_.size()));
    }
}