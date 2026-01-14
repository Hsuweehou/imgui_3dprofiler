#include "CameraScannerUI.h"
#include "glog/logging.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // 设置控制台代码页为 UTF-8，以正确显示中文
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 初始化 glog
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1; // 同时输出到控制台

    try {
        // 创建 UI 实例
        CameraScannerUI ui;

        // 初始化
        if (!ui.Initialize(1280, 720, "相机扫描系统")) {
            std::cerr << "Failed to initialize UI" << std::endl;
            return -1;
        }

        // 运行主循环
        ui.Run();

        // 清理
        ui.Cleanup();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        LOG(ERROR) << "Exception: " << e.what();
        return -1;
    }

    google::ShutdownGoogleLogging();
    return 0;
}
