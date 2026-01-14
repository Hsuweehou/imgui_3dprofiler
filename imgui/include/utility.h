#ifndef UTILITYMY_H
#define UTILITYMY_H
#pragma once
#include <GLFW/glfw3.h>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include "imgui.h"
#include "imguifile/ImGuiFileDialog.h"
#include <iostream>
//#include "imgui_impl_glfw.h"
//#include "imgui_impl_opengl3.h"
//#include <imgui_internal.h>
#include <vector>
#include <string>
#include <mutex>
#include <glog/logging.h>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace utility
{
    // 将opencv的mat转换为opengl可接受的数据类型
    GLuint matToTexture(const cv::Mat &mat);

    
    // imgui 对文件夹路径选择的支持
    void SelectFolderButton(
        std::string &filepath,
        const char *button,
        const char *dialogKey,
        const char *title);

    //
    bool concat(const cv::Mat& left,const cv::Mat& right,cv::Mat& dst);


    struct LogEntry {
    google::LogSeverity severity;
    std::string message;
    std::string timestamp;
    };

}

class RealtimeLogSink : public google::LogSink {
public:
    void send(google::LogSeverity severity, const char* full_filename,
        const char* base_filename, int line,
        const struct ::tm* tm_time,
        const char* message, size_t message_len) override {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_time);
        log_buffer_.push_back({
            severity,
            std::string(message, message_len),
            std::string(time_buf)
            });
        new_log_available_ = true;
    }

    bool CheckNewLogs() {
        bool ret = new_log_available_;
        new_log_available_ = false;
        return ret;
    }

    std::vector<utility::LogEntry> GetLogs() {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        return log_buffer_;
    }

public:
    std::mutex buffer_mutex_;
    std::vector<utility::LogEntry> log_buffer_;
private:
    bool new_log_available_ = false;
};

#endif