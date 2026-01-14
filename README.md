# 相机扫描系统 GUI


基于ImGui实现的相机扫描系统图形用户界面

## 主要功能

- 初始化扫描器
- 连接/断开扫描器
- 开始/停止扫描
- 自动获取并保存扫描数据
- 保存点云数据保存（TIFF 格式，XYZ 坐标）、灰度图像保存（TIFF 格式）
- 数据保存路径可配置（通过 `all_path_config.json`）


## 依赖

- ImGui
- OpenCV 4.5.1
- glog
- nlohmann/json
- L series line stripe laser camera SDK v4
- serial
- C++17

## build
- CMake 3.18 或更高版本
- Visual Studio 2019 或 Visual Studio 2022
- C++17 编译器

### how

1. 创建构建目录：
```bash
mkdir build && cd build
```

2. 配置 CMake：
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

3. 使用 Visual Studio 打开生成的解决方案文件（.sln）进行编译

### 输出文件

编译完成后，可执行文件位于 `build/Bin/Release/` 或 `build/Bin/Debug/` 目录下。

构建系统会自动将以下文件复制到输出目录：
- 必要的 DLL 文件（从 `SDK_C++/bin`）
- 字体文件 `msyh.ttc`（从 `assert/` 目录）

## 配置文件

### 路径配置
系统会从以下位置查找 `all_path_config.json` 配置文件：
1. `config_path + "../all_path_config.json"`
2. `config_path + "all_path_config.json"`
3. `../ScannerConfig/all_path_config.json`

配置文件应包含 `data_root_path` 字段，指定扫描数据的保存根路径。

### 扫描器配置
扫描器配置文件位于 `ScannerConfig/` 目录下。

## 使用说明

1. **启动程序**：运行编译后的可执行文件
2. **配置路径**：在控制面板中输入配置路径，点击"应用配置路径"
3. **初始化**：点击"初始化"按钮初始化扫描器
4. **连接**：点击"连接"按钮连接扫描器
5. **扫描**：点击"开始扫描"开始扫描，扫描完成后点击"停止扫描"自动保存数据
6. **查看日志**：在 LOG 面板中查看系统运行日志
7. **查看数据**：在数据面板中查看扫描数据信息
