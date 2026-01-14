/*******************************************************************************
 *   FILENAME:      mitsubishi_plc_fx_link.cpp
 *
 *   AUTHORS:       Zhiwen Dai    START DATE: Saturday, April 12th 2025
 *
 *   LAST MODIFIED: Friday, May 23rd 2025, 2:34:54 pm
 *
 *   Copyright (c) 2023 - 2025 DepthVision Limited
 *
 *   CONTACT:       zwdai@hkclr.hk
 *******************************************************************************/

#include "../include/mitsubishi_plc_fx_link.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "../serial/include/serial/serial.h"
#include <iomanip>
#include <vector>


bool MitsubishiPlcFxLink::Connect(const std::string& port, uint32_t baudrate) {
    try {
        m_serial = std::make_unique<serial::Serial>(
            port, baudrate, serial::Timeout::simpleTimeout(1000), serial::sevenbits,
            serial::parity_odd, serial::stopbits_one);
        m_serial->close();
        m_serial->open();
        if (!m_serial->isOpen()) {
            throw std::runtime_error("test failed: port is not open");
        }
    } catch (const std::exception& e) {
        std::cerr << "MitsubishiPlcFxLink port open failed: " + std::string(e.what()) << std::endl;
        m_serial = nullptr;
        return false;
    }
    std::cout << "MitsubishiPlcFxLink port open success!" << std::endl;
    return true;
}

MitsubishiPlcFxLink::~MitsubishiPlcFxLink() {
    if (m_serial == nullptr) {
        std::cout << "port is not open, auto quit" << std::endl;
        return;
    }
    if (m_serial->isOpen()) {
        m_serial->close();
        std::cout << "port close success!" << std::endl;
    }
}

std::string MitsubishiPlcFxLink::byteToAsciiHex(uint8_t val) {
    // 0x0~0xF => '0'~'F'
    static const char* hexDigits = "0123456789ABCDEF";

    std::string result;
    result.push_back(hexDigits[(val >> 4) & 0x0F]);  // 高4位
    result.push_back(hexDigits[val & 0x0F]);         // 低4位
    return result;
}

std::string MitsubishiPlcFxLink::wordToAsciiHex(uint16_t val) {
    // 0~65535 => "0000"~"FFFF"
    static const char* hexDigits = "0123456789ABCDEF";
    std::string result(4, '0');
    // 依次填充高4位 -> 次高4位 -> 次低4位 -> 低4位
    result[0] = hexDigits[(val >> 12) & 0x0F];
    result[1] = hexDigits[(val >> 8) & 0x0F];
    result[2] = hexDigits[(val >> 4) & 0x0F];
    result[3] = hexDigits[val & 0x0F];
    return result;
}

bool MitsubishiPlcFxLink::Move(double distance) {
    auto init_pos = GetPosition();

    auto expected_distance = Round0p005(distance);

    if (expected_distance + init_pos > kXMax) {
        expected_distance = Round0p005(kXMax - init_pos);
    };

    if (expected_distance + init_pos < kXMin) {
        expected_distance = Round0p005(kXMin - init_pos);
    };

    // std::cout << "expected distance: " << expected_distance << std::endl;

    auto expected_distance_ticks = Tick0p005(expected_distance);

    // std::cout << "expected distance ticks: " << expected_distance_ticks << std::endl;

    // if (distance < 0 || distance > 200) return false;

    // 按照实际需求计算 32 位整数值
    uint32_t distance_int = static_cast<uint32_t>(expected_distance_ticks);

    // 将 32 位整数拆分成低 16 位和高 16 位
    uint16_t lower = static_cast<uint16_t>(distance_int & 0xFFFF);           // 低 16 位
    uint16_t higher = static_cast<uint16_t>((distance_int >> 16) & 0xFFFF);  // 高 16 位

    // 假定 WordWrite 支持传入多个数据项，这里将两部分数据放到一个初始化列表中发送
    WordWrite(0, 255, "D142", {lower, higher});
    // std::string address = distance > 0 ? "M32" : "M33";
    std::string address = "M32";
    // distance = std::abs(distance);
    BitWrite(0, 255, address, {true});

    auto time_estimate = 2 * std::abs(expected_distance / GetSpeed());

    // Sleep(1000 * time_estimate);
    auto start = std::chrono::system_clock::now();
    while (true) {
        // std::cout << "getting pos" << std::endl;
        auto pos = GetPosition();
        // std::cout << "getting pos finished" << std::endl;
        if (abs(pos - init_pos - expected_distance) < 0.001) {
            // std::cout << "sleep 1000" << std::endl;
            Sleep(1000);
            break;
        };
        if (std::chrono::duration<double>(std::chrono::system_clock::now() - start).count() >
            time_estimate)
            break;
        // std::cout << "sleep 50" << std::endl;
        Sleep(50);
    }
    BitWrite(0, 255, address, {false});
    return abs(GetPosition() - init_pos - expected_distance) < 0.001;
}

bool MitsubishiPlcFxLink::MoveTo(double position_in, double& position_out) {
    auto res = Move(position_in - GetPosition());
    position_out = GetPosition();
    return res;
}

bool MitsubishiPlcFxLink::SetAccel(double accel) {
    if (accel < 0 || accel > 1000) return false;
    uint32_t accel_int = static_cast<uint32_t>(accel * 200);
    uint16_t lower = static_cast<uint16_t>(accel_int & 0xFFFF);
    uint16_t higher = static_cast<uint16_t>((accel_int >> 16) & 0xFFFF);
    WordWrite(0, 255, "D144", {lower, higher});
    return true;
};

std::string MitsubishiPlcFxLink::padDeviceAddr(const std::string& addr) {
    // 例如 "D100" 长度=4 => 我们希望返回 "D0100"
    //     "M1"   => "M0001"
    //     "D01234" 多了则截取成 "D0123" (示例中取前5)
    if (addr.size() >= 5) {
        return addr.substr(0, 5);
    }

    // 第一个字符视为设备类型，如 'D', 'M' 等
    char devType = addr[0];
    // 其余部分视为数字
    std::string digits = (addr.size() > 1) ? addr.substr(1) : "";

    // 填充到总计4位
    while (digits.size() < 4) {
        digits.insert(digits.begin(), '0');
    }

    // 拼回到一起，保证总长度5
    std::string ret = std::string(1, devType) + digits.substr(0, 4);
    return ret;
}

std::string MitsubishiPlcFxLink::GetReadCommand(uint8_t station, uint8_t pc,
                                                const std::string& deviceAddress,
                                                uint8_t deviceCount, bool isBitRead) {
    // 1) 将 station、pc、deviceCount 均转为 2 位 ASCII 十六进制字符串
    //    例如 station=30 => 十六进制 0x1E => "1E" => 0x31 0x45
    std::string stationStr = byteToAsciiHex(station);
    std::string pcStr = byteToAsciiHex(pc);
    std::string countStr = byteToAsciiHex(deviceCount);

    // 2) “WR” 指令是固定的 "W" (0x57) + "R" (0x52)
    //    报文等待值这里固定使用 '3' (ASCII 0x33) 做示例
    std::string commandType = isBitRead ? "BR" : "WR";
    std::string waitChar = "3";

    // 3) 将起始软元件地址补足到5个字符 (如 "D100" => "D0100")
    std::string devAddr5 = padDeviceAddr(deviceAddress);

    // 4) 拼接完整报文（忽略和校验），形式可参考：
    //    stationStr + pcStr + "WR" + "3" + devAddr5 + countStr
    std::string command =
        std::string(1, ENQ) + stationStr + pcStr + commandType + waitChar + devAddr5 + countStr;

    // 打印示例报文
    // std::cout << "WordRead 命令: " << command << std::endl;

    return command;
}

bool MitsubishiPlcFxLink::IsConnected() {
    if (!m_serial) return false;
    return m_serial->isOpen();
}

bool MitsubishiPlcFxLink::Read(uint8_t station, uint8_t pc, const std::string& deviceAddress,
                               uint8_t deviceCount, bool isBitRead, std::vector<uint16_t>& values) {
    if (!IsConnected()) {
        std::cerr << "Read: serial is not open" << std::endl;
        return false;
    }
    try {
        auto command = GetReadCommand(station, pc, deviceAddress, deviceCount, isBitRead);
        m_serial->flushInput();
        // 5) 发送报文
        size_t bytesWritten = m_serial->write(command);
        if (bytesWritten != command.size()) {
            std::cerr << "Read 命令写入不完整，预期写入 " << command.size() << " 字节，实际写入 "
                      << bytesWritten << " 字节" << std::endl;
            return false;
        }

        // 6) 简单读取响应（示例直接一次性读取 100 字节）
        std::string response = m_serial->read(30);
        if (response.empty()) {
            std::cerr << "Read 未收到PLC响应" << std::endl;
            return false;
        }

        // 7) 打印或解析响应（此处仅打印示例）
        // std::cout << "PLC 响应数据: " << response << std::endl;

        values.clear();
        // 如果是 bit 读取，那么解析 bit
        if (isBitRead) {
            std::vector<bool> bits = parseBitReadResponse(response);
            // 这里你可以把 bits 存起来、或者返回给上层
            // std::cout << "读取到 " << bits.size() << " 个位，内容：";
            for (auto bit : bits) {
                std::cout << (bit ? '1' : '0');
                values.push_back(static_cast<uint16_t>(bit));
            }
            std::cout << std::endl;
        } else {
            std::vector<uint16_t> words = parseWordReadResponse(response);
            for (auto word : words) {
                values.push_back(word);
            }
            // word 读取的解析逻辑另写函数
            // std::cout << "Read 响应数据: " << response << std::endl;
        }

        return true;

    } catch (const std::exception& ex) {
        std::cerr << "Read 异常: " << ex.what() << std::endl;
        return false;
    }
}

std::string MitsubishiPlcFxLink::GetBitWriteCommand(uint8_t station, uint8_t pc,
                                                    const std::string& deviceAddress,
                                                    const std::vector<bool>& bitsToWrite) {
    // 1) 转为ASCII
    std::string stationStr = byteToAsciiHex(station);
    std::string pcStr = byteToAsciiHex(pc);

    // 写入位数
    if (bitsToWrite.size() > 255) {
        std::cerr << "BitWrite: 超过最大255位限制，目前 size=" << bitsToWrite.size() << std::endl;
        return false;
    }
    uint8_t deviceCount = static_cast<uint8_t>(bitsToWrite.size());
    std::string countStr = byteToAsciiHex(deviceCount);

    // 2) 指令固定 "BW"
    std::string commandType = "BW";
    std::string waitChar = "3";  // 示例中固定 3

    // 3) 设备地址补足5字符
    std::string devAddr5 = padDeviceAddr(deviceAddress);

    // 4) 将传入的 bitsToWrite 拼装为字符 '0'/'1'
    std::string bitsData;
    bitsData.reserve(bitsToWrite.size());
    for (bool bit : bitsToWrite) {
        bitsData.push_back(bit ? '1' : '0');
    }

    // 5) 组装命令串
    //    ENQ + station + pc + BW + "3" + devAddr5 + countStr + bitsData
    std::string command = std::string(1, static_cast<char>(ENQ)) + stationStr + pcStr +
                          commandType + waitChar + devAddr5 + countStr + bitsData;

    // 打印示例报文
    // std::cout << "BitWrite 命令: " << command << std::endl;

    return command;
}

std::string MitsubishiPlcFxLink::GetWordWriteCommand(uint8_t station, uint8_t pc,
                                                     const std::string& deviceAddress,
                                                     const std::vector<uint16_t>& wordsToWrite) {
    // 1) 将 station、pc 转为 2位ASCII 十六进制
    std::string stationStr = byteToAsciiHex(station);
    std::string pcStr = byteToAsciiHex(pc);

    // 2) 写入的字数限制：由于指令中的count也用 1 字节表示，所以最多 255 个字
    if (wordsToWrite.size() > 255) {
        std::cerr << "WordWrite: 超过最大255字限制，目前 size=" << wordsToWrite.size() << std::endl;
        return "";  // 返回空串，表示失败
    }
    uint8_t deviceCount = static_cast<uint8_t>(wordsToWrite.size());
    std::string countStr = byteToAsciiHex(deviceCount);

    // 3) 指令固定使用 "WW"
    std::string commandType = "WW";
    // 报文等待值这里固定为 '3'，如同你的示例
    std::string waitChar = "3";

    // 4) 设备地址补足到5字符 (如 "D100" => "D0100")
    std::string devAddr5 = padDeviceAddr(deviceAddress);

    // 5) 拼装要写入的数据，每个 word => 4 ASCII Hex
    std::string wordsData;
    wordsData.reserve(wordsToWrite.size() * 4);
    for (auto w : wordsToWrite) {
        wordsData += wordToAsciiHex(w);
    }

    // 6) 组装最终命令：
    //    ENQ + station + pc + "WW" + "3" + devAddr5 + countStr + wordsData
    std::string command = std::string(1, static_cast<char>(ENQ)) + stationStr + pcStr +
                          commandType + waitChar + devAddr5 + countStr + wordsData;

    // 日志输出
    // std::cout << "WordWrite 命令: " << command << std::endl;
    return command;
}

bool MitsubishiPlcFxLink::WordWrite(uint8_t station, uint8_t pc, const std::string& deviceAddress,
                                    const std::vector<uint16_t>& wordsToWrite) {
    if (!IsConnected()) {
        std::cerr << "WordWrite: serial is not open" << std::endl;
        return false;
    }
    try {
        // 1) 组装命令
        auto command = GetWordWriteCommand(station, pc, deviceAddress, wordsToWrite);
        // 如果返回空串表示出错
        if (command.empty()) {
            return false;
        }
        // std::cout << "WordWrite 命令: " << command << std::endl;
        m_serial->flushInput();

        // 2) 写入串口
        size_t bytesWritten = m_serial->write(command);
        if (bytesWritten != command.size()) {
            std::cerr << "WordWrite 命令写入不完整，预期写入 " << command.size()
                      << " 字节，实际写入 " << bytesWritten << " 字节" << std::endl;
            return false;
        }

        // 3) 读取响应（简化，读取最多100字节）
        std::string response = m_serial->read(10);
        if (response.empty()) {
            std::cerr << "WordWrite 未收到PLC响应" << std::endl;
            return false;
        }

        // 4) 打印响应或进行解析
        // std::cout << "PLC 响应数据: " << response << std::endl;
        return true;

    } catch (const std::exception& ex) {
        std::cerr << "WordWrite 异常: " << ex.what() << std::endl;
        return false;
    }
}

/**
 * @brief BW指令（位单位的成批写入）。
 *        bitsToWrite 的大小决定了要写多少位，支持写最多255位（受一个字节ASCII限制）。
 *        每一位在数据区表示为 '0' 或 '1'。
 */
bool MitsubishiPlcFxLink::BitWrite(uint8_t station, uint8_t pc, const std::string& deviceAddress,
                                   const std::vector<bool>& bitsToWrite) {
    if (!IsConnected()) {
        std::cerr << "BitWrite: serial is not open" << std::endl;
        return false;
    }
    try {
        auto command = GetBitWriteCommand(station, pc, deviceAddress, bitsToWrite);
        // std::cout << "BitWrite 命令: " << command << std::endl;
        m_serial->flushInput();

        // 6) 写入串口
        // std::cout << "write start" << std::endl;
        size_t bytesWritten = m_serial->write(command);
        if (bytesWritten != command.size()) {
            std::cerr << "BitWrite 命令写入不完整，预期写入 " << command.size()
                      << " 字节，实际写入 " << bytesWritten << " 字节" << std::endl;
            return false;
        }
        // std::cout << "write end" << std::endl;

        // std::cout << "read response start" << std::endl;

        // 7) 读取响应（简化处理）
        std::string response = m_serial->read(10);
        if (response.empty()) {
            std::cerr << "BitWrite 未收到PLC响应" << std::endl;
            return false;
        }

        // std::cout << "read response end" << std::endl;

        // 输出响应
        // std::cout << "PLC 响应数据: " << response << std::endl;
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "BitWrite 异常: " << ex.what() << std::endl;
        return false;
    }
}

/**
 * @brief 解析三菱FX返回的位读取报文。
 *        假设格式:
 *           [STX=0x02] [station(2 ASCII)] [pc(2 ASCII)] [bitData(N个 '0' or '1')] [ETX=0x03].
 */
std::vector<bool> MitsubishiPlcFxLink::parseBitReadResponse(const std::string& response) {
    std::vector<bool> bits;

    // 基础检查：至少要包含 STX + station(2字节) + pc(2字节) + ETX => 1 + 2 + 2 + 1 = 6 字符.
    if (response.size() < 6) {
        std::cerr << "[parseBitReadResponse] 报文太短，无法解析." << std::endl;
        return bits;
    }

    // 检查首字节是否是 STX(0x02)，末字节是否是 ETX(0x03).
    if (static_cast<unsigned char>(response[0]) != 0x02) {
        std::cerr << "[parseBitReadResponse] 缺少 STX(0x02)." << std::endl;
        return bits;
    }
    if (static_cast<unsigned char>(response.back()) != 0x03) {
        std::cerr << "[parseBitReadResponse] 缺少 ETX(0x03)." << std::endl;
        return bits;
    }

    // 示例中 station 占 2 个 ASCII，pc 占 2 个 ASCII.
    // STX(1字节) + station(2字节) + pc(2字节) = 前 5 字节.
    // 剩下到倒数第 1 字节(ETX) 前的那段视为 bitData.
    //   索引:  0    [ 1..2 ]  [ 3..4 ]   [ 5..(n-2) ]   [ n-1 ].
    //         STX   station   pc        bitData       ETX.

    size_t dataStart = 1 + 2 + 2;          // 跳过 STX + station + pc.
    size_t dataEnd = response.size() - 1;  // 最后 ETX 不算入数据.

    // 遍历 bitData 区域：每个字符应为 '0' 或 '1'.
    for (size_t i = dataStart; i < dataEnd; ++i) {
        char c = response[i];
        if (c == '0') {
            bits.push_back(false);
        } else if (c == '1') {
            bits.push_back(true);
        } else {
            // 若遇到非 '0'/'1' 字符，可选择报错或跳过.
            std::cerr << "[parseBitReadResponse] 非法字符: " << c << " (索引 " << i << ")"
                      << std::endl;
            // 也可以 return 空向量表示解析失败.
        }
    }

    return bits;
}

/**
 * @brief 将长度=4的 ASCII 十六进制字符串转换为 uint16_t
 */
uint16_t MitsubishiPlcFxLink::parseHexWord(const std::string& s) {
    if (s.size() != 4) {
        // 简单防护
        throw std::runtime_error("[parseHexWord] 输入字符串长度不为4: " + s);
    }
    // 可以使用 stoi，也可使用手动解析
    // stoi(s, 0, 16) 将 "ABCD" 转为 0xABCD
    // 遇到非法字符时会抛出 std::invalid_argument
    return static_cast<uint16_t>(std::stoi(s, nullptr, 16));
}

/**
 * @brief 解析三菱 FX 返回的“字读取”报文
 *        假设格式:
 *         [STX=0x02]
 *         [station(2 ASCII)] [pc(2 ASCII)]
 *         [wordData(N组，每组4 ASCII Hex)]
 *         [ETX=0x03]
 */
std::vector<uint16_t> MitsubishiPlcFxLink::parseWordReadResponse(const std::string& response) {
    std::vector<uint16_t> words;

    // 1. 基础检查：最短也要包含 STX + station(2) + pc(2) + ETX = 1 + 2 + 2 + 1 = 6 字符
    if (response.size() < 6) {
        std::cerr << "[parseWordReadResponse] 报文太短，无法解析." << std::endl;
        return words;
    }

    // 2. 检查首字节是否 STX(0x02) & 末字节是否 ETX(0x03)
    if (static_cast<unsigned char>(response[0]) != 0x02) {
        std::cerr << "[parseWordReadResponse] 缺少 STX(0x02)." << std::endl;
        return words;
    }
    if (static_cast<unsigned char>(response.back()) != 0x03) {
        std::cerr << "[parseWordReadResponse] 缺少 ETX(0x03)." << std::endl;
        return words;
    }

    // 3. 跳过 STX(1 字节), station(2 ASCII), pc(2 ASCII)
    //    dataStart => 响应中实际字数据开始的位置
    //    dataEnd   => 去掉 ETX 的末尾
    size_t dataStart = 1 + 2 + 2;          // 跳过 [STX][station][pc]
    size_t dataEnd = response.size() - 1;  // 最后 ETX 不算入数据

    // 如果没有字数据，则直接返回空向量
    if (dataEnd <= dataStart) {
        return words;  // 空
    }

    // 4. 提取字数据分段
    //    每个 word 对应 4 个 ASCII 字符 => 例如 "1234" => 0x1234
    size_t dataLen = dataEnd - dataStart;
    // 检查长度能否被 4 整除
    if (dataLen % 4 != 0) {
        std::cerr << "[parseWordReadResponse] wordData 长度非4的倍数，解析可能出错." << std::endl;
    }

    // 以 4 个字符为一组，解析为 uint16_t
    std::string wordData = response.substr(dataStart, dataLen);
    size_t wordCount = dataLen / 4;
    words.reserve(wordCount);

    for (size_t i = 0; i < wordCount; ++i) {
        std::string chunk = wordData.substr(i * 4, 4);
        try {
            uint16_t val = parseHexWord(chunk);
            words.push_back(val);
        } catch (const std::exception& ex) {
            std::cerr << "[parseWordReadResponse] 解析失败: " << ex.what() << " (chunk: " << chunk
                      << ")" << std::endl;
            // 此处可以选择 return 空 或者继续解析剩余
            return {};
        }
    }

    return words;
}
void MitsubishiPlcFxLink::PrintHexCommand(const std::string& command) {
    for (unsigned char c : command) {
        // 输出两个宽度的十六进制数，填充0
        std::cout << std::setw(2) << std::setfill('0') << std::hex << std::uppercase
                  << static_cast<int>(c) << " ";
    }
    std::cout << std::endl;
};