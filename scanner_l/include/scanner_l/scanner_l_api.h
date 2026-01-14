#ifndef SCANNER_L_API_H
#define SCANNER_L_API_H

#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <filesystem>
#include <io.h>

//#include "libmodbus/modbus.h"
#include "scanner_l/type.h"
#include "scanner_l/scanner_all_data.h"
#include "scanner_l/scan_io.h"
#include "../../plc_serial/include/mitsubishi_plc_fx_link.h"
#include "./motion_conf.h"
#include "FileWatcher.h"

// using namespace AIeveR::Device;
// using namespace AIeveR::Algorithm;

#define debug_show_cam_params 0
#define dynamic_change_cam_params 0

#ifdef _WIN32
#define SANY_GRPC_SCANNER_L_EXPORTS __declspec(dllexport)
#elif
    #define SANY_GRPC_SCANNER_L_EXPORTS 
#endif

int read_work_distance(std::string config_rootpath, std::string config_filename, double& dist);

class SANY_GRPC_SCANNER_L_EXPORTS ScannerLApi {
public:
    ScannerLApi();

    ~ScannerLApi();

    void SetConfigRootPath(const std::string& in_root_path);

    int Init();

    int Connect();

    int Reset();

    int Start();

    int End();

    int GetAllData(std::vector<std::vector<cv::Point3f>>& out_pc_vec, 
                std::vector<std::vector<uint8_t>>& out_gray_vec,
                std::vector<std::vector<int32_t>>& out_encoder_vec,
                std::vector<std::vector<uint32_t>>& out_framecnt_vec);

    void camera_params_load();

    //ÐÂ¼Ó

    void Encoder_Handle_Data(Scanner_All_Data& all_data , const std::string& scanner_num, AIeveR_Point3D& mv_vec);

    int disconnect();

public:
    std::map<std::string, int> original_params;

    std::map<std::string, int> changed_params;

private:

    platfromOffset platfrom_offset;

    std::string config_root_path_;

    std::vector<AIScannerDeveloper*> scanner_l_ptr_vec_;
    std::vector<std::string> scanner_l_ipv4_vec_;
    std::vector<AIeveR_Point3D*> scanner_l_move_vec_;

    std::vector<AIeveR_ScannerInfo*> scanner_l_info_;
    std::vector<AIeveR_HostInfo*> scanner_l_config_vec_;

    // std::vector<unsigned int> max_capture_frame_cnt_vec_;

    std::vector<double> profile_stitch_distances;

    std::vector<cv::Mat> scanner_l_rt_vec_;

    // scanners' min and max zrange.
    std::vector<std::vector<int>> scanner_l_zrange_vec_;

    unsigned int main_scan_index_;

    bool b_backward_= false;

    float x_distance_ = 0.0;

    std::vector<unsigned int> forward_encoder_values_;

    std::vector<float> mv_dists_ = {0,0};

    float calibration_z_compensation_ = 0.0;
    
    cv::Point3f backward_compensation_pt_;

    cv::Mat global_transform_mat_ = cv::Mat::eye(4, 4, CV_64FC1);

    int load_config_file(std::string config_rootpath, std::string config_filename, AIeveR_HostInfo& out_scanner_config, AIeveR_ScannerInfo& out_scanner_l_info,std::vector<int>& out_zrange, cv::Mat& out_multi_calib_rt, bool& main_scan, double& profile_stitch_dist, int& callback_cnt, AIeveR_Point3D& out_scanner_l_mv_vec, AIeveR_Point3D& scan_mov_vec);

    void release_scanner_l_ptr();

};


#endif // SANY_GRPC_SCANNER_L_SCANNER_L_API_H