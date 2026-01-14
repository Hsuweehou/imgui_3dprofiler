#include "scanner_l/Scanner_Server.h"

namespace {
    void SaveXYZData(std::vector<cv::Point3f>& XYZ_Data, std::vector<uint8_t>& gray_vec, std::vector<int32_t>& encoder_vec,
        std::vector<uint32_t>& frame_cnt_vec, const char* File_name)
    {
        FILE* fp = fopen(File_name, "wb");
        fprintf(fp, "ply\n");
        fprintf(fp, "format binary_little_endian 1.0\n");
        fprintf(fp, "comment File generated\n");
        fprintf(fp, "element vertex %llu\n", XYZ_Data.size());
        fprintf(fp, "property float x\n");
        fprintf(fp, "property float y\n");
        fprintf(fp, "property float z\n");
        fprintf(fp, "property uint8 intensity\n");
        fprintf(fp, "property uint32 encoder\n");
        fprintf(fp, "property uint32 framecnt\n");
        fprintf(fp, "end_header\n");
        for (int i = 0; i < XYZ_Data.size(); i++)
        {
            float tmp2[3] = { XYZ_Data[i].x, XYZ_Data[i].y, XYZ_Data[i].z };
            fwrite(tmp2, sizeof(float), 3, fp);

            uint8_t gray = gray_vec[i];
            fwrite(&gray, sizeof(uint8_t), 1, fp);

            int32_t i_encoder = encoder_vec[i];
            fwrite(&i_encoder, sizeof(int32_t), 1, fp);

            uint32_t i_frame_cnt = frame_cnt_vec[i];
            fwrite(&i_frame_cnt, sizeof(uint32_t), 1, fp);

        }
        fclose(fp);
    }

    void SaveXYZData_happly(std::vector<cv::Point3f>& XYZ_Data, std::vector<uint8_t>& gray_vec, std::vector< int>& encoder_vec,
        std::vector<unsigned int>& frame_cnt_vec, const char* File_name)
    {
        m_PlyFormat format = m_PlyFormat::BINARY;
        bool status = WritePCToPLY(XYZ_Data.data(), XYZ_Data.size(), File_name, nullptr, 0, encoder_vec.data(), encoder_vec.size(), frame_cnt_vec.data(), frame_cnt_vec.size(), gray_vec.data(), gray_vec.size());

    }
    cv::Mat save_ply2tiff(std::vector<float>& x_coords, std::vector<float>& y_coords, std::vector<float>& z_coords,
        int width, int height, const std::string& filename) {
        std::string save_filename = filename;
        cv::Mat tiff_image(height, width, CV_32FC3);

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                int idx = row * width + col;
                cv::Vec3f& pixel = tiff_image.at<cv::Vec3f>(row, col);
                pixel[0] = x_coords[idx];  // X -> 通道2 - b
                pixel[1] = y_coords[idx];  // Y -> 通道1 - g
                pixel[2] = z_coords[idx];  // Z -> 通道0 - r
            }
        }
        // 保存TIFF文件
        std::vector<int> compression_params = { cv::IMWRITE_TIFF_COMPRESSION, 1 }; // 可选压缩参数
        cv::imwrite(save_filename, tiff_image, compression_params);
        return tiff_image;
    }
    cv::Mat save_gray2tiff(std::vector<uint8_t>& data, int width, int height, const std::string& filename) {
        std::string save_filename = filename;

        cv::Mat tiff_image(height, width, CV_8UC1, data.data());

        // 保存TIFF文件
        std::vector<int> compression_params = { cv::IMWRITE_TIFF_COMPRESSION, 1 }; // 可选压缩参数
        cv::imwrite(save_filename, tiff_image, compression_params);
        return tiff_image;
    }
    
    using json = nlohmann::json;

    ConfigData parse_config(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open config file");
        }
        return nlohmann::json::parse(file).get<ConfigData>();
    }


    void mkdir_documentory_(int scanner_num) {

        for (int i = 0; i < scanner_num; i++) {

            std::string batch_dir = "scanner_" + std::to_string(i);
            try {
                std::filesystem::create_directories(batch_dir);
            }
            catch (const std::filesystem::filesystem_error& e) {
                LOG(ERROR) << "Mkdir failed [" << batch_dir << "]: "
                    << e.what() << "\n";
            }
        }
    }

    void ply_data_calibr_file(int n) {

        // 处理两个扫描文件 
        // for (int loop = 0; loop < n; ++loop) {
        std::string batch_dir = "scanner_calibr";

        std::string multi_cam_alltogether = "pointclouds_alltogether_" + std::to_string(n) + ".ply";

        std::filesystem::path src = multi_cam_alltogether;
        std::filesystem::path dst = std::filesystem::path(batch_dir) / multi_cam_alltogether;

        try {
            if (!std::filesystem::exists(src)) {
                LOG(ERROR) << "src file is not exist: " << src << "\n";
            }

            // 移动前删除已存在的目标文件 
            if (std::filesystem::exists(dst)) std::filesystem::remove(dst);

            std::filesystem::rename(src, dst);
            LOG(INFO) << "Moving: " << src << " => " << dst << "\n";

        }
        catch (const std::filesystem::filesystem_error& e) {
            LOG(ERROR) << "manipulate file failed: " << e.what() << "\n";
        }
        // }
    }

    void ply_data_batch_file(std::string n, int scanner_num, std::string filename_tail) {

        // 处理两个扫描文件 
        for (int scan = 0; scan < scanner_num; ++scan) {
            std::string batch_dir = "scanner_" + std::to_string(scan);
            std::string filename = "pointclouds_loop_" + n + "_scan_" + std::to_string(scan) + filename_tail;

            std::filesystem::path src = filename;
            std::filesystem::path dst = std::filesystem::path(batch_dir) / filename;

            try {
                if (!std::filesystem::exists(src)) {
                    LOG(ERROR) << "src file is not exist: " << src << "\n";
                }

                // 移动前删除已存在的目标文件 
                if (std::filesystem::exists(dst)) std::filesystem::remove(dst);

                std::filesystem::rename(src, dst);
                LOG(INFO) << "Moving: " << src << " => " << dst << "\n";

            }
            catch (const std::filesystem::filesystem_error& e) {
                LOG(ERROR) << "manipulate file failed: " << e.what() << "\n";
            }
        }
    }
}

int Scanner_Server::Config_Scanner(RealtimeLogSink& sink){
    //auto logs = sink.GetLogs();

    path_store_pc = "./";
    LOG(INFO) << " path_store_pc:" << path_store_pc;

#if SAVE_OVERALL_PC
    std::vector<uint8_t>().swap(grays_tmp);
    std::vector<uint8_t>().swap(grays);
    std::vector<int32_t>().swap(encoders);
    std::vector<int32_t>().swap(encoders_tmp);
    std::vector<uint32_t>().swap(framecnts);
    std::vector<uint32_t>().swap(framecnts_tmp);
    point_cloud_ptr_ = std::make_shared<std::vector<cv::Point3f>>();
    std::vector<cv::Point3f>().swap(*point_cloud_ptr_);
    point_cloud_ptr_tmp = std::make_shared<std::vector<cv::Point3f>>();
    std::vector<cv::Point3f>().swap(*point_cloud_ptr_tmp);
#endif
    path_config_path = path_store_pc + "../ScannerConfig/all_path_config.json";
    //path_config_path = "./../ScannerConfig/all_path_config.json";
    std::ifstream f(path_config_path);
    LOG(INFO) << "path_config_path: " << path_config_path << "\n";
    if (!f.is_open()) {
        LOG(INFO) << "ERROR - Fail to open json file " << path_config_path;
        return -1;
    }
    json data = json::parse(f);
    f.close();

    data_root_path = data["data_root_path"];
    config_plc_filename = data["config_plc_filename"];
    set_config_root_path = data["set_config_root_path"];
    shared_memory_name_pc = data["shared_memory_name_pc"];
    shared_memory_name_gray = data["shared_memory_name_gray"];

    scanner_sys_.SetConfigRootPath(set_config_root_path + "ScannerConfig/"); // Must set config path first.
    int flag_scan = scanner_sys_.Init();

    plc_setting_path = set_config_root_path + "/ScannerConfig/" + config_plc_filename;
    Solution sol_current;
    try {
        ConfigData config = parse_config(plc_setting_path);
        //config.moving_speed = 150;
        LOG(INFO) << "Current Solution: " << config.current_solution_id;
        //sol_current.set_speed = config.moving_speed;
        LOG(INFO) << "Current speed: " << sol_current.set_speed << " = " << config.moving_speed;
        config.GetCurrentSolution(sol_current);
        LOG(INFO) << "set speed: " << sol_current.set_speed;
        LOG(INFO) << "Solution set speed: " << sol_current.set_speed;
        LOG(INFO) << "Solution " << sol_current.id << " name " << sol_current.name << " scanner y design position: "
            << sol_current.y_coord_info.scanner_design_pt << "mm";
        LOG(INFO) << "Solution " << sol_current.id << " name " << sol_current.name << " scanner x loading position: "
            << sol_current.x_coord_info.loading_pt << "mm";
        LOG(INFO) << "Solution " << sol_current.id << " name " << sol_current.name << " scanner x line_scan_end position: "
            << sol_current.x_coord_info.line_scan_end_pt << "mm";
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    ready_position = sol_current.x_coord_info.loading_pt;
    move_left_position = sol_current.x_coord_info.line_scan_end_pt;
    move_right_position = sol_current.x_coord_info.loading_pt;
    moving_speed = 100;

    LOG(INFO) << "init return: " << flag_scan;
    if (flag_scan != 0) {
        LOG(ERROR) << "ERROR - Failed to init scanner system: " << flag_scan;
        return -3;
    }
    //创建文件夹
    mkdir_documentory_(1);
    return 0;
}

int Scanner_Server::Connect_Scanner(){
    int flag_scan = scanner_sys_.Connect();
    LOG(INFO) << "connection return: " << flag_scan;
    if (flag_scan != 0) {
        // open failed
        LOG(ERROR) << "ERROR - Failed to connect to scanner system: " << flag_scan;
        return -1;
    }
    return 0;
}

int Scanner_Server::Start_Scanner(){
    int flag_scan = scanner_sys_.Start();
    LOG(INFO) << "Start return: " << flag_scan;
    if (flag_scan != 0) {
            // open failed
            LOG(ERROR) << "ERROR - Failed to trigger scanner system: " << flag_scan;
            return -1;
    }
    return 0;
}

int Scanner_Server::End_Scanner(){
    std::string date_time_str;
    get_system_time(date_time_str);
    int flag_scan = scanner_sys_.End();
    LOG(INFO) << "End return: " << flag_scan;
    if (flag_scan != 0) {
        LOG(ERROR) << "ERROR - Failed to stop scanner system's laser: " << flag_scan;
    }
    std::shared_ptr<std::vector<cv::Point3f>> point_cloud_ptr_tmp;
    std::vector<uint8_t> grays_tmp;
    std::vector<int32_t> encoders_tmp;
    std::vector<uint32_t> framecnts_tmp;
    std::vector<uint8_t>().swap(grays_tmp);
    std::vector<int32_t>().swap(encoders_tmp);
    std::vector<uint32_t>().swap(framecnts_tmp);
    point_cloud_ptr_tmp = std::make_shared<std::vector<cv::Point3f>>();
    std::vector<cv::Point3f>().swap(*point_cloud_ptr_tmp);

    std::vector<std::vector<cv::Point3f>> i_pc_vec;
    std::vector<std::vector<uint8_t>> i_gray_vec;
    std::vector<std::vector<int32_t>> i_encoder_vec;
    std::vector<std::vector<uint32_t>> i_framecnt_vec;
    std::vector<float> i_pc_x;
    std::vector<float> i_pc_y;
    std::vector<float> i_pc_z;
    std::vector<uint8_t> i_gray;
    cv::Mat tiff_image;
    cv::Mat tiff_image_gray;
    scanner_sys_.GetAllData(i_pc_vec, i_gray_vec, i_encoder_vec, i_framecnt_vec);
    auto save_ply_starttime = std::chrono::system_clock::now();
    // TODO: Store pc in corresponding container.
    LOG(INFO) << "Save pc to files";
    LOG(INFO) << "i_pc_vec.size(): " << i_pc_vec.size() << "\n";
    //std::string shared_memory_name_pc = "pointclouds_loop_" + std::to_string(loop_cnt) + "_scan_0_pc";
    //std::string shared_memory_name_gray = "pointclouds_loop_" + std::to_string(loop_cnt) + "_scan_0_gray";
    std::string shared_memory_tiff_pc = data_root_path + "pointclouds_loop_" + date_time_str + "_scan_0_pc_shared.tiff";
    std::string shared_memory_tiff_gray = data_root_path + "pointclouds_loop_" + date_time_str + "_scan_0_gray_shared.tiff";
    uint64 shared_memory_size_pc = 0;
    uint64 shared_memory_size_gray = 0;
    std::string path_laser_scan_pc_thr;
    std::string path_laser_scan_pc_nothr;
    std::string path_laser_scan_tiff_pc;
    std::string path_laser_scan_tiff_gray;
    bool write_memory_sign = true;
    for (int j = 0; j < i_pc_vec.size(); j++) {
        LOG(INFO) << "J: " << j << "\n";
        // std::string path_laser_scan_pc = path_store_pc + "pointclouds_loop_" + std::to_string(loop_cnt) 
        //     + "_scan_"+ std::to_string(j) + ".ply";
        path_laser_scan_pc_thr = data_root_path + "pointclouds_loop_" + date_time_str + "_scan_"+ std::to_string(j) + "_thread.ply";
        path_laser_scan_pc_nothr = data_root_path + "pointclouds_loop_" + date_time_str + "_scan_"+ std::to_string(j) + "_nothread.ply";
        path_laser_scan_tiff_pc = data_root_path + "pointclouds_loop_" + date_time_str + "_scan_"+ std::to_string(j) + "_pc.tiff";
        path_laser_scan_tiff_gray = data_root_path + "pointclouds_loop_" + date_time_str + "_scan_"+ std::to_string(j) + "_gray.tiff";

        point_cloud_ptr_tmp->insert(point_cloud_ptr_tmp->end(), i_pc_vec[j].begin(), i_pc_vec[j].end());
        grays_tmp.insert(grays_tmp.end(), i_gray_vec[j].begin(), i_gray_vec[j].end());
        encoders_tmp.insert(encoders_tmp.end(), i_encoder_vec[j].begin(), i_encoder_vec[j].end());
        framecnts_tmp.insert(framecnts_tmp.end(), i_framecnt_vec[j].begin(), i_framecnt_vec[j].end());
        for (int k = 0; k < i_pc_vec[j].size(); k++) {
            i_pc_x.push_back(i_pc_vec[j][k].x);
            i_pc_y.push_back(i_pc_vec[j][k].y);
            i_pc_z.push_back(i_pc_vec[j][k].z);
        }
        for (int g = 0; g < i_gray_vec[j].size(); g++) {
            i_gray.push_back(i_gray_vec[j][g]);
        }
        LOG(INFO) << "i_pc_x.size(): " << i_pc_x.size() << "\n";
        LOG(INFO) << "i_pc_y.size(): " << i_pc_y.size() << "\n";
        LOG(INFO) << "i_pc_z.size(): " << i_pc_z.size() << "\n";
        if (i_pc_x.size() == 0 || i_pc_y.size() == 0 || i_pc_z.size() == 0 || i_gray.size() == 0) {
            write_memory_sign = false;
            LOG(INFO) << "scanner " << j << " recive 0 data!";
            continue;
        }
        
        tiff_image = save_ply2tiff(i_pc_x, i_pc_y, i_pc_z, 3200, i_pc_z.size() / 3200, path_laser_scan_tiff_pc);
        tiff_image_gray = save_gray2tiff(i_gray, 3200 , i_gray.size() / 3200, path_laser_scan_tiff_gray);
        SaveXYZData_happly(*point_cloud_ptr_tmp, grays_tmp, encoders_tmp, framecnts_tmp, path_laser_scan_pc_nothr.c_str());
        std::thread save_ply([&]() {
            SaveXYZData_happly(*point_cloud_ptr_tmp, grays_tmp, encoders_tmp, framecnts_tmp, path_laser_scan_pc_thr.c_str());
            LOG(INFO) << "saved ply ...!";
            });
        save_ply.detach();

    }
    //save batch data
    //std::string batch_num = std::to_string(loop_cnt);
    /*ply_data_batch_file(batch_num, i_pc_vec.size(), ".ply");
    ply_data_batch_file(batch_num, i_pc_vec.size(), "_pc.tiff");
    ply_data_batch_file(batch_num, i_pc_vec.size(), "_gray.tiff");*/
    // writeMemory_gray(imgDir_gray);
    // readMemory_gray();
    if (write_memory_sign) {
        /*int sharedmemory_status = writeMemory_pc(tiff_image, shared_memory_name_pc, "file_name", shared_memory_size_pc);
        LOG(INFO) << "shared_memory_size_pc: " << shared_memory_size_pc;
        readMemory_pc(shared_memory_name_pc, shared_memory_tiff_pc, shared_memory_size_pc);

        sharedmemory_status = writeMemory_gray(tiff_image_gray, shared_memory_name_gray, "file_name", shared_memory_size_gray);
        LOG(INFO) << "shared_memory_size_gray: " << shared_memory_size_gray;
        readMemory_gray(shared_memory_name_gray, shared_memory_tiff_gray, shared_memory_size_gray);*/
        /*ply_data_batch_file(batch_num, i_pc_vec.size(), "_pc_shared.tiff");
        ply_data_batch_file(batch_num, i_pc_vec.size(), "_gray_shared.tiff");*/
        //SaveXYZData_happly(*point_cloud_ptr_tmp, grays_tmp, encoders_tmp, framecnts_tmp, path_laser_scan_pc_nothr.c_str());
        /* std::thread save_ply([&]() {
            SaveXYZData_happly(*point_cloud_ptr_tmp, grays_tmp, encoders_tmp, framecnts_tmp, path_laser_scan_pc_thr.c_str());
            LOG(INFO) << "saved ply ...!";
            });
        save_ply.detach();*/
    }
        

#if SAVE_OVERALL_PC
    for (size_t i = 0; i < i_pc_vec.size(); i++)
    {
        point_cloud_ptr_tmp->insert(point_cloud_ptr_tmp->end(), i_pc_vec[i].begin(), i_pc_vec[i].end());
        grays_tmp.insert(grays_tmp.end(), i_gray_vec[i].begin(), i_gray_vec[i].end());
        encoders_tmp.insert(encoders_tmp.end(), i_encoder_vec[i].begin(),i_encoder_vec[i].end());
        framecnts_tmp.insert(framecnts_tmp.end(), i_framecnt_vec[i].begin(),i_framecnt_vec[i].end());
    }
    LOG(INFO) << "saving overall data...";
    std::string multi_cam_alltogether = path_store_pc + "pointclouds_alltogether_" + std::to_string(loop_cnt) + ".ply";
    SaveXYZData_happly(*point_cloud_ptr_tmp, grays_tmp, encoders_tmp, framecnts_tmp, multi_cam_alltogether.c_str());
    LOG(INFO) << "saving done, moving data...";
    ply_data_calibr_file(loop_cnt);
#endif

    std::vector<std::vector<cv::Point3f>>().swap(i_pc_vec);
    std::vector<std::vector<uint8_t>>().swap(i_gray_vec);
    std::vector<std::vector<int32_t>>().swap(i_encoder_vec);
    std::vector<std::vector<uint32_t>>().swap(i_framecnt_vec);

    std::vector<float>().swap(i_pc_x);
    std::vector<float>().swap(i_pc_y);
    std::vector<float>().swap(i_pc_z);

    std::vector<uint8_t>().swap(i_gray);
    std::vector<uint8_t>().swap(grays_tmp);
    std::vector<int32_t>().swap(encoders_tmp);
    std::vector<uint32_t>().swap(framecnts_tmp);

    point_cloud_ptr_tmp = std::make_shared<std::vector<cv::Point3f>>();
    std::vector<cv::Point3f>().swap(*point_cloud_ptr_tmp);
#if SAVE_OVERALL_PC
    std::vector<uint8_t>().swap(grays_tmp);
    std::vector<uint8_t>().swap(grays);
    std::vector<int32_t>().swap(encoders);
    std::vector<int32_t>().swap(encoders_tmp);
    std::vector<uint32_t>().swap(framecnts);
    std::vector<uint32_t>().swap(framecnts_tmp);

    point_cloud_ptr_ = std::make_shared<std::vector<cv::Point3f>>();
    std::vector<cv::Point3f>().swap(*point_cloud_ptr_);
    point_cloud_ptr_tmp = std::make_shared<std::vector<cv::Point3f>>();
    std::vector<cv::Point3f>().swap(*point_cloud_ptr_tmp);
#endif
    auto save_ply_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - save_ply_starttime).count();
    LOG(INFO) << "save ply times: " << save_ply_diff << " ms\n";
    return 0;
}

int Scanner_Server::Disconnect_Scanner(){
    int disconnect_status = scanner_sys_.disconnect();
    if(disconnect_status != 0)
        LOG(ERROR) << "ERROR - Failed to disconnect scanner system's laser: " << disconnect_status;
    else
        LOG(INFO) << "INFO - disconnect scanner system's laser: " << disconnect_status;
    return 0;
}

//int main() {
//    Scanner_Server test;
//    return 0;
//}