/*******************************************************************************
 *   FILENAME:      test_link.cpp
 *
 *   AUTHORS:       Zhiwen Dai    START DATE: Saturday, April 12th 2025
 *
 *   LAST MODIFIED: Friday, May 23rd 2025, 2:31:44 pm
 *
 *   Copyright (c) 2023 - 2025 DepthVision Limited
 *
 *   CONTACT:       zwdai@hkclr.hk
 *******************************************************************************/

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include "mitsubishi_plc_fx_link.h"
#include "test_context.h"

TEST(TestLink, PadDeviceAddr) {
    ASSERT_TRUE(MitsubishiPlcFxLink::padDeviceAddr("M100") == "M0100");
}

TEST(TestLink, GetReadCommand) {
    {
        auto command = MitsubishiPlcFxLink::GetReadCommand(0, 255, "D100", 1, false);
        MitsubishiPlcFxLink::PrintHexCommand(command);
    }
    {
        auto command = MitsubishiPlcFxLink::GetReadCommand(0, 255, "D100", 1, true);
        MitsubishiPlcFxLink::PrintHexCommand(command);
    }
}

TEST(TestLink, GetWriteCommand) {
    {
        auto command = MitsubishiPlcFxLink::GetBitWriteCommand(0, 255, "D100", {false});
        MitsubishiPlcFxLink::PrintHexCommand(command);
    }
    {
        auto command = MitsubishiPlcFxLink::GetWordWriteCommand(0, 255, "D100", {1000});
        MitsubishiPlcFxLink::PrintHexCommand(command);
    }
}

TEST(TestLink, ParseReadResponse) {
    {
        // 构造一个测试响应字符串，格式假设如下：
        // [STX (0x02)] [station: "00"] [pc: "FF"] [bitData: "10101100"] [ETX (0x03)]
        // 其中 bitData 区域为读取的位数据，每个字符 '0' 或 '1'
        std::string response;
        response.push_back(0x02);     // STX
        response.append("00");        // station（示例为 "00"）
        response.append("FF");        // PC号（示例为 "FF"）
        response.append("10101100");  // 位数据（共8位，示例）
        response.push_back(0x03);     // ETX
        auto result = MitsubishiPlcFxLink::parseBitReadResponse(response);
        for (auto bit : result) {
            std::cout << (bit ? '1' : '0');
        }
        std::cout << std::endl;
        auto expected = std::vector<bool>{true, false, true, false, true, true, false, false};
        ASSERT_EQ(result, expected);
    }
    {
        std::string response;
        response.push_back(0x02);         // STX
        response.append("00");            // station
        response.append("FF");            // pc
        response.append("123400A51234");  // wordData: 三个字
        response.push_back(0x03);         // ETX
        auto result = MitsubishiPlcFxLink::parseWordReadResponse(response);
        for (auto word : result) {
            std::cout << std::hex << word << " ";
        }
        std::cout << std::endl;
        auto expected = std::vector<uint16_t>{0x1234, 0x00A5, 0x1234};
        ASSERT_EQ(result, expected);
    }
}

TEST(TestLink, ConnectTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link("COM5", 9600);
    // std::cout << "reading from D100:" << std::endl;
    // link.Read(0, 255, "D100", 1, false);
    link.WordWrite(0, 255, "D100", {1000});
    std::cout << "reading from D100:" << std::endl;
    std::vector<uint16_t> data;
    link.Read(0, 255, "D100", 1, false, data);
    std::cout << "data: " << data[0] << std::endl;

    std::cout << "reset:" << std::endl;
    link.BitWrite(0, 255, "M20", {true});

    link.WordWrite(0, 255, "D270", {1});
    link.Read(0, 255, "D270", 1, false, data);
    std::cout << "data: " << data[0] << std::endl;
}

TEST(TestLink, ControlTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link("COM5", 9600);

    link.ResetPosition();
    auto pos = link.GetPosition();
    std::cout << "pos: " << pos << std::endl;
    link.SetSpeed(150);
    // 循环寸动
    for (int i = 0; i < 2; i++) {
        link.Move(100);
        link.Move(-100);
        pos = link.GetPosition();
    }

    std::cout << "pos: " << pos << std::endl;
}

TEST(TestLink, GetPositionTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link("COM5", 9600);

    auto pos = link.GetPosition();
    std::cout << "realtime position: " << pos << std::endl;
}

TEST(TestLink, ResetPositionTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link;
    link.Connect("COM5", 9600);
    link.SetSpeed(150);
    link.ResetPosition();
    auto pos = link.GetPosition();
    std::cout << "pos: " << pos << std::endl;
}

TEST(TestLink, MultiCamAutoTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link;
    link.Connect("COM5", 9600);
    link.SetSpeed(150);
    auto init_pos = link.GetPosition();
    std::cout << "init_pos: " << init_pos << "\n";
    if (init_pos <= 200.0 || init_pos >= 600.0) {
        link.ResetPosition();
        std::cout << "return to init pos\n";
    }
    Sleep(2000);
    link.Move(280);
    std::cout << "move to ready pos\n";
    // 循环寸动
    for (int i = 0; i < 2; i++) {
        link.Move(300);
        std::cout << "forward\n";
        link.Move(-300);
        std::cout << "backward\n";
    }
    std::cout << "Stop moving\n";
}

TEST(TestLink, SetSpeed) {
    MitsubishiPlcFxLink link("COM5", 9600);
    link.SetSpeed(150);  // passed
    auto speed = link.GetSpeed();
    std::cout << "speed: " << speed << std::endl;
    ASSERT_EQ(speed, 150.0);
}

TEST(TestLink, Reset) {
    MitsubishiPlcFxLink link("COM5", 9600);
    ASSERT_TRUE(link.ResetPosition());
}

TEST(TestLink, MoveTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link("COM5", 9600);
    // link.ResetPosition();
    link.SetSpeed(12.999);
    auto speed = link.GetSpeed();
    std::cout << "speed: " << speed << std::endl;
    // auto init_pos = link.GetPosition();
    // std::cout << "init_pos: " << init_pos << "\n";
    ASSERT_TRUE(link.ResetPosition());
    std::cout << "reset" << std::endl;

    auto pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0, 0.001);

    link.Move(8.01);
    // pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;

    // ASSERT_NEAR(pos, 8.01, 0.001);

    link.Move(-8.012);
    // pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;

    // ASSERT_NEAR(pos, 0, 0.001);

    ASSERT_TRUE(link.Move(0.001));
    // pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0, 0.001);

    ASSERT_TRUE(link.Move(0.131));
    // pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0.13, 0.01);

    ASSERT_TRUE(link.Move(0.166));
    // pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0.13 + 0.165, 0.01);

    ASSERT_TRUE(link.Move(-0.014));
    // pos = link.GetPosition();
    // std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0.13 + 0.165 - 0.015, 0.001);
}

TEST(TestLink, MoveToTest) {
    auto info_list = serial::list_ports();
    for (auto& info : info_list) {
        std::cout << info.port << std::endl;
    }
    MitsubishiPlcFxLink link("COM5", 9600);
    // link.ResetPosition();
    link.SetSpeed(12.999);
    auto speed = link.GetSpeed();
    std::cout << "speed: " << speed << std::endl;
    // auto init_pos = link.GetPosition();
    // std::cout << "init_pos: " << init_pos << "\n";
    ASSERT_TRUE(link.ResetPosition());
    std::cout << "reset" << std::endl;

    double pos;
    ASSERT_TRUE(link.MoveTo(8.01, pos));
    std::cout << "pos = " << pos << std::endl;

    // ASSERT_NEAR(pos, 8.01, 0.001);

    ASSERT_TRUE(link.MoveTo(8.012, pos));
    // pos = link.GetPosition();
    std::cout << "pos = " << pos << std::endl;

    // ASSERT_NEAR(pos, 0, 0.001);

    ASSERT_TRUE(link.MoveTo(28.3, pos));
    // pos = link.GetPosition();
    std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0, 0.001);

    ASSERT_TRUE(link.MoveTo(25.35, pos));
    // pos = link.GetPosition();
    std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0.13, 0.01);

    ASSERT_TRUE(link.MoveTo(39.07, pos));
    // pos = link.GetPosition();
    std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0.13 + 0.165, 0.01);

    ASSERT_TRUE(link.MoveTo(0.014, pos));
    // pos = link.GetPosition();
    std::cout << "pos = " << pos << std::endl;
    // ASSERT_NEAR(pos, 0.13 + 0.165 - 0.015, 0.001);
}