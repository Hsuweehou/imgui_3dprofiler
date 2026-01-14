#ifndef SCANNER_SERVER_H
#define SCANNER_SERVER_H

#pragma once
#include "scanner_l/scanner_l_api.h"
//#include "scanner_l/scan_share_memory.h"
#include "glog/logging.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include  <nlohmann/json.hpp>
#include "../../plc_serial/include/mitsubishi_plc_fx_link.h"
#include "utility.h"

#define SAVE_OVERALL_PC 0

namespace {
    void get_system_time(std::string& YMD_str) {
        
        time_t rawtime;
        struct tm * timeinfo;
        char buffer [100];

        time (&rawtime);
        timeinfo = localtime (&rawtime);

        strftime (buffer, 100, "%Y%m%d_%H%M%S", timeinfo);

        YMD_str = buffer;
    }

    void setup_glog(std::string log_path) {
        while (1) {
            std::size_t pos = log_path.find_first_of("/");
            if (pos == std::string::npos)
                break;
            log_path.replace(pos, 1, "\\");
        }
        std::string command = "mkdir " + log_path;
        // LOG(INFO) << "execute command:" << command;
        system(command.c_str());

        std::string filename_log = log_path + "LOG_ScannerCameraServer_";
        google::SetLogDestination(google::GLOG_INFO, filename_log.c_str());
        //google::InstallFailureSignalHandler();
        //google::InstallFailureWriter(&SanyCamera::LogSignalHandler);
        
        if (google::IsGoogleLoggingInitialized())
        {
            google::ShutdownGoogleLogging();
        }
        google::InitGoogleLogging("ScannerCameraServer");
    }

}

class Scanner_Server{

public:
    Scanner_Server(){}

    ~Scanner_Server(){}

    int Config_Scanner(RealtimeLogSink& sink);

    int Connect_Scanner();

    int Start_Scanner();

    int End_Scanner();

    int Disconnect_Scanner();



private:
    ScannerLApi scanner_sys_;

    std::string path_store_pc;
    std::string path_config_path;
    std::string data_root_path;
    std::string config_plc_filename;
    std::string set_config_root_path;
    std::string shared_memory_name_pc;
    std::string shared_memory_name_gray;

    std::string plc_setting_path;
    Solution sol_current;
    float ready_position;
    float move_left_position;
    float move_right_position;
    int moving_speed;

#if SAVE_OVERALL_PC
    std::shared_ptr<std::vector<cv::Point3f>> point_cloud_ptr_; 
    std::shared_ptr<std::vector<cv::Point3f>> point_cloud_ptr_tmp;
    std::vector<uint8_t> grays;
    std::vector<uint8_t> grays_tmp;
    std::vector<int32_t> encoders;
    std::vector<int32_t> encoders_tmp;
    std::vector<uint32_t> framecnts;
    std::vector<uint32_t> framecnts_tmp;
#endif

};

#endif