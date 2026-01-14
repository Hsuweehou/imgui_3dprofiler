#ifndef SCANNER_ALL_DATA_H
#define SCANNER_ALL_DATA_H
#pragma once
#include <iostream>
#include "../../SDK_C++/include/AIeveR.h"
#include "../../SDK_C++/include/AIeveR_Head/AIScannerDeveloper.h"
#include <stdio.h>
#include <winsock2.h>
#include <windows.h>

using namespace AIeveR::Device;
using namespace AIeveR::Algorithm;

class Scanner_All_Data{
public:
    Scanner_All_Data(){}

    ~Scanner_All_Data(){
    }

    //147
    std::vector<uint8_t> ALL_GRAY_VEC_;
    std::vector<AIeveR_Point3F> ALL_PC_VEC_;
    std::vector < uint32_t> FRAME_VEC_;
    std::vector < int32_t> ENCODER_VEC_;

    //存放点云
    std::vector<uint8_t> ALL_GRAY_VEC_SAVE{};
    std::vector<AIeveR_Point3F> ALL_PC_VEC_SAVE{};
    std::vector < uint32_t> FRAME_VEC_SAVE{};
    std::vector < int32_t> ENCODER_VEC_SAVE{};

    // //回调计数值设置
    // void SetCallbackCounts(const int& in_callbackcount) { callBackCount_ = in_callbackcount; }
    // int GetCallbackCounts() { return callBackCount_; }
    // //回调次数设置
    // void SetNeededCallbackCounts(const int& in_callbackcount) { needCallbackCount_ = in_callbackcount; }
    // int GetNeededCallbackCounts() { return needCallbackCount_; }

// private:
//     int callBackCount_ = 0;
//     int needCallbackCount_ = 100;

};


#endif