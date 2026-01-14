/*******************************************************************************
 *   FILENAME:      mitsubishi_plc_fx_link.h
 *
 *   AUTHORS:       Zhiwen Dai    START DATE: Saturday, April 12th 2025
 *
 *   LAST MODIFIED: Friday, May 23rd 2025, 2:35:34 pm
 *
 *   Copyright (c) 2023 - 2025 DepthVision Limited
 *
 *   CONTACT:       zwdai@hkclr.hk
 *******************************************************************************/

#ifndef MITSUBISHI_PLC_FX_LINK_H
#define MITSUBISHI_PLC_FX_LINK_H
#pragma once
#include <cstdint>
#include <memory>
#include <string>
// #include <ctime>
#include <chrono>
#include "serial/serial.h"  // 使用 serial 库
#include <winsock2.h>
#include <Windows.h>
#include <iostream>

class MitsubishiPlcFxLink {
   public:
    /**
     * @brief 连接到PLC
     * @param port 串口号，例如 "COM3" 或 "/dev/ttyS0"
     * @param baudrate 波特率，默认为9600..
     */
    bool Connect(const std::string& port, uint32_t baudrate);

    MitsubishiPlcFxLink() {};

    MitsubishiPlcFxLink(const std::string& port, uint32_t baudrate) {
        if (!Connect(port, baudrate)) {
            std::cerr << "MitsubishiPlcFxLink port open failed" << std::endl;
        }
    };

    ~MitsubishiPlcFxLink();

    /**
     * @brief 批量读取（Batch Read）函数
     * @param station    站号 (0~255)，将被转换为2字节ASCII（十六进制）
     * @param pc         PC号 (0~255)，将被转换为2字节ASCII（十六进制），常见是 "FF"
     * @param deviceAddress 起始软元件地址，需转成5个ASCII字节（如 "D0100" = 0x44 0x30 0x31 0x30
     * 0x30）
     * @param deviceCount   软元件点数 (0~255)，将被转换为2字节ASCII（十六进制）
     * @param isBitRead  是否为位读取 (true) 还是字读取 (false)
     * @return true 成功，false 失败
     */
    bool Read(uint8_t station, uint8_t pc, const std::string& deviceAddress, uint8_t deviceCount,
              bool isBitRead, std::vector<uint16_t>& values);

    /**
     * @brief 将地址字符串转为 5 个字符（不做额外校验，简单补零）
     * 例如: "D100" => "D0100" => 对应 ASCII 0x44 0x30 0x31 0x30 0x30
     * 若已是5个字符，则直接截取前5个返回
     */
    static std::string padDeviceAddr(const std::string& addr);

    /**
     * @brief 打印十六进制命令
     * @param command 命令字符串
     */
    static void PrintHexCommand(const std::string& command);

    /**
     * @brief 获取读取命令
     *
     * @param station 站号
     * @param pc PC号
     * @param deviceAddress 软元件地址
     * @param deviceCount 软元件数目
     * @param isBitRead 是否为位读取，true 为位读取，false 为字读取
     * @return std::string 命令
     */
    static std::string GetReadCommand(uint8_t station, uint8_t pc, const std::string& deviceAddress,
                                      uint8_t deviceCount, bool isBitRead);

    /**
     * @brief 获取 写 bit 命令
     *
     * @param station 站号
     * @param pc PC号
     * @param deviceAddress 软元件地址
     * @param bitsToWrite 需要写的位列表
     * @return std::string 命令
     */
    static std::string GetBitWriteCommand(uint8_t station, uint8_t pc,
                                          const std::string& deviceAddress,
                                          const std::vector<bool>& bitsToWrite);
    /**
     * @brief 写入命令
     *
     * @param station 站号
     * @param pc PC号
     * @param deviceAddress 软元件地址
     * @param bitsToWrite 需要写的位列表
     * @return true 写入成功
     * @return false
     */
    bool BitWrite(uint8_t station, uint8_t pc, const std::string& deviceAddress,
                  const std::vector<bool>& bitsToWrite);

    /**
     * @brief 获取 写 word （2字节）命令
     *
     * @param station 站号
     * @param pc PC号
     * @param deviceAddress 软元件地址
     * @param wordsToWrite 需要写的字列表
     * @return std::string 命令
     */
    static std::string GetWordWriteCommand(uint8_t station, uint8_t pc,
                                           const std::string& deviceAddress,
                                           const std::vector<uint16_t>& wordsToWrite);

    /**
     * @brief 写入命令
     *
     * @param station 站号
     * @param pc PC号
     * @param deviceAddress 软元件地址
     * @param wordsToWrite 需要写的字列表
     * @return true 写入成功
     * @return false
     */
    bool WordWrite(uint8_t station, uint8_t pc, const std::string& deviceAddress,
                   const std::vector<uint16_t>& wordsToWrite);

    /**
     * @brief 解析 BitRead 响应，将其中的位数据解析为 std::vector<bool>
     * @param response 完整的原始字符串 (含 STX/ETX)
     * @return 一个 bool 向量，表示每个位的状态；若解析失败则返回空向量
     */
    static std::vector<bool> parseBitReadResponse(const std::string& response);

    /**
     * @brief 解析 WordRead 响应，将其中的字数据解析为 std::vector<uint16_t>
     * @param response 完整的原始字符串 (含 STX/ETX)
     * @return 一个 16-bit 字的向量；若解析失败则返回空向量
     */
    static std::vector<uint16_t> parseWordReadResponse(const std::string& response);

    /// 将 0~65535 的数值转成4位 ASCII 十六进制字符串，例如 0x1E => "0001"
    static std::string wordToAsciiHex(uint16_t val);

    /// 复位 (等待 20 s)
    bool ResetPosition() {
        // double init_pos = GetPosition();
        BitWrite(0, 255, "M20", {true});
        // Sleep(20 * 1000);
        double time_estimate = 20;
        auto start = std::chrono::system_clock::now();
        while (true) {
            auto pos = GetPosition();
            if (pos == 0) {
                Sleep(2000);
                std::cout << "reset success" << std::endl;
                break;
            };
            if (std::chrono::duration<double>(std::chrono::system_clock::now() - start).count() >
                time_estimate) {
                std::cout << "reset time out" << std::endl;
                break;
            }
            Sleep(50);
        }
        return GetPosition() == 0;
    };

    /**
     * @brief 设置速度
     *
     * @param speed 速度 (mm/s)
     */
    bool SetSpeed(double speed) {
        if (speed < 0 || speed > 1000) return false;
        uint32_t speed_int = static_cast<uint32_t>(speed * 200);
        uint16_t lower = static_cast<uint16_t>(speed_int & 0xFFFF);
        uint16_t higher = static_cast<uint16_t>((speed_int >> 16) & 0xFFFF);
        WordWrite(0, 255, "D128", {lower, higher});
        return fabs(speed - GetSpeed()) < 0.001;
    };

    double GetSpeed() {
        std::vector<uint16_t> speed_vec;
        Read(0, 255, "D128", 2, false, speed_vec);
        int speed_int = speed_vec[0] | (speed_vec[1] << 16);
        auto speed = static_cast<double>(speed_int) / 200.0;
        return speed;
    }

    /**
     * @brief 移动
     *
     * @param distance 距离 (mm)
     * @return true
     * @return false
     */
    bool Move(double distance);

    /**
     * @brief 移动到指定位置
     *
     * @param position_in 输入位置
     * @param position_out 输出位置
     * @return true
     * @return false
     */
    bool MoveTo(double position_in, double& position_out);

    /**
     * @brief 设置加速度
     *
     * @param accel 加速度 (mm/s^2)
     * @return true
     * @return false
     */
    bool SetAccel(double accel);

    /**
     * @brief 获取当前位置
     *
     * @return double 位置
     */
    double GetPosition() {
        std::vector<uint16_t> position;
        Read(0, 255, "D140", 2, false, position);
        int32_t position_int = position[0] | (position[1] << 16);
        return static_cast<double>(position_int) / 200.0;
    };

    /**
     * @brief 串口是否连接
     */
    bool IsConnected();

   private:
    static constexpr char SOH = 0x01;
    static constexpr char STX = 0x02;
    static constexpr char ETX = 0x03;
    static constexpr char EOT = 0x04;
    static constexpr char ENQ = 0x05;
    static constexpr char ACK = 0x06;

    const double kXMin = 0;
    const double kXMax = 631;

    /// 串口对象
    std::unique_ptr<serial::Serial> m_serial = nullptr;

    /// 将 0~255 的数值转成2位 ASCII 十六进制字符串，例如 0x1E => "1E".
    static std::string byteToAsciiHex(uint8_t val);

    /**
     * @brief 将长度=4 的 ASCII 十六进制字符串转换为 uint16_t
     *        例如 "0123" => 0x0123, "ABCD" => 0xABCD.
     */
    static uint16_t parseHexWord(const std::string& s);

    /// 将 x 四舍五入到 0.005 的整数倍
    inline double Round0p005(double x) {
        /* 乘 200 → 变成 0.005 的倍数计数；
           std::round() 做 IEEE-754 “四舍五入到最近、半数舍入到 +∞”；
           再除回 200 得到最终值。.*/
        return std::round(x * 200.0) / 200.0;
    }

    inline int Tick0p005(double x) {
        /* 乘 200 → 变成 0.005 的倍数计数；
           std::round() 做 IEEE-754 “四舍五入到最近、半数舍入到 +∞”；
           再除回 200 得到最终值。.*/
        return static_cast<int>(std::round(x * 200.0));
    }
};

#endif
