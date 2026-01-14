/*******************************************************************************
 *   FILENAME:      main.cpp
 *  
 *   AUTHORS:       Zhiwen Dai    START DATE: Saturday, April 12th 2025
 *  
 *   LAST MODIFIED: Saturday, April 12th 2025, 3:38:43 pm
 *  
 *   Copyright (c) 2023 - 2025 DepthVision Limited
 *  
 *   CONTACT:       zwdai@hkclr.hk
*******************************************************************************/
#pragma once
#include <winsock2.h>
#define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include "test_context.h"
 #include <iostream>
 #include <string>

 // 第一个传入参数为测试数据根目录, 是必须的
 // 还可更多参数, 为 gtest 框架可识别的参数, 如: --gtest_filter=PointTransformDouble 过滤器,
 // 可指定要执行的测试用例, 默认执行所有
 int main(int argc, char** argv) {
     // 获取终端输入输出编码
     UINT input_cp = GetConsoleCP();
     UINT output_cp = GetConsoleOutputCP();
 
     // 设置终端输入输出编码为 UTF-8
     SetConsoleCP(CP_UTF8);
     SetConsoleOutputCP(CP_UTF8);
 
     ::testing::InitGoogleTest(&argc, argv);
     auto res = RUN_ALL_TESTS();
 
 
     // 恢复原来的代码页
     SetConsoleCP(input_cp);
     SetConsoleOutputCP(output_cp);
     return res;
 }