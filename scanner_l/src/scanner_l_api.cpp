#include "scanner_l/scanner_l_api.h"
#include "scanner_l/happly.h"
#include "glog/logging.h"
#include  <nlohmann/json.hpp>
// #include <fstream>

namespace {

    std::vector<std::string> SCANNER_CONFIG_FILE_VEC = {
        "scanner_0.json",
    };
    std::vector<std::string> SCANNER_CONFIG_FILE_TXT = {
        "scanner_0.txt",
    };
    Scanner_All_Data test_147;

    std::vector<Scanner_All_Data> all_PC_data;
    using json = nlohmann::json;
    //回调次数
    std::atomic<int> g_callBackCount_0 = 0;
    int g_needCallbackCount_ = 30;
    std::atomic<bool> thr_flag = false;
    std::vector<AIeveR_Point3D*> scan_move_vec_;

    std::vector<double> profile_stitch_dist = { 0.004 };
    int scanner_work_distance = read_work_distance("../ScannerConfig/", SCANNER_CONFIG_FILE_VEC[0], profile_stitch_dist[0]);
    //默认工作距离
    PostProcessing global_postProcessing_(scanner_work_distance);///series_L10140_workdist
    int setLaserIntense = 0;
    int batch_value = 0;
    std::string scanner_param_path = "../ScannerConfig/" + SCANNER_CONFIG_FILE_TXT[0];
    FileWatcher fw(scanner_param_path, 200);
    
}

int read_work_distance(std::string config_rootpath, std::string config_filename, double& profile_stitch_dist){

    std::string workdist_path = config_rootpath + config_filename;
    std::ifstream config_file(workdist_path);

    LOG(INFO) << "workdist_path: " << workdist_path << "\n";
    if (!config_file.is_open()) {
        LOG(INFO) << "ERROR - Fail to open workdist_path " << workdist_path;
        return -1;
    }
    json readed_data = json::parse(config_file);
    config_file.close();
    int ret = readed_data["working_distance"];
    profile_stitch_dist = readed_data["profile_stitch_distance"];
    return ret;
}

void printProgressBar(int progress, int total, int barWidth)
{
    float percentage = (float)progress / total;
    int filledLength = barWidth * percentage;
    printf("\r[");
    for (int i = 0; i < barWidth; i++) {
        if (i < filledLength) {
            printf("#");
        } else {
            printf("-");
        }
    }
    printf("] %.1f%%", (percentage) * 100);
    fflush(stdout);
}

ScannerLApi::ScannerLApi() {
    //相机实例
    std::vector<AIScannerDeveloper*> ().swap(scanner_l_ptr_vec_);
    //搜索方式相机的信息
    std::vector<AIeveR_ScannerInfo*> ().swap(scanner_l_info_);
    std::vector<AIeveR_HostInfo*> ().swap(scanner_l_config_vec_);
    //直连方式两个相机的信息
    std::vector<std::string> ().swap(scanner_l_ipv4_vec_);
    //两个相机RT配置
    std::vector<cv::Mat> ().swap(scanner_l_rt_vec_);
    //运动方向
    std::vector<AIeveR_Point3D*> ().swap(scanner_l_move_vec_);
    //Z轴的范围，过滤不在同一平面的点，比如地板，模拟边界之类的
    std::vector<std::vector<int>> ().swap(scanner_l_zrange_vec_);
    std::map<std::string, int>().swap(original_params);
    std::map<std::string, int>().swap(changed_params);
    config_root_path_ = "./";
    main_scan_index_ = 0;
}

ScannerLApi::~ScannerLApi() {
    release_scanner_l_ptr();
}

//不改动
void ScannerLApi::SetConfigRootPath(const std::string& in_root_path) {
    config_root_path_ = in_root_path;
    //scanner_param_path = "../ScannerConfig/" + SCANNER_CONFIG_FILE_TXT[0];
    /*FileWatcher fw(scanner_param_path, 200);
    LOG(INFO) << "set FileWatcher \n";*/
}

void ScannerLApi::camera_params_load() {
    bool show_flag = false;
#if debug_show_cam_params
    for (const auto& pair : original_params) {
        LOG(INFO) << "key: " << pair.first << ", value: " << pair.second << "\n";
    }
#endif
    while (1) {
        if (fw.getModifiedSign()) {
            load_params(scanner_param_path, changed_params);
            auto diff = compare_maps(changed_params, original_params);
            //auto diff = compare_maps1(changed_params, original_params);

            LOG(INFO) << "Differences:\n";
            for (const auto& pair : diff) {
                LOG(INFO) << "Key: " << pair.first << ", Value: " << pair.second << "\n";
                original_params[pair.first] = pair.second;
                ErrorStatus set_status = scanner_l_ptr_vec_[0]->setParameterValue(pair.first, pair.second);
                LOG(INFO) << set_status.errorCode << " | " << set_status.errorDescription;
            }
            fw.setModifiedSign(false);
            show_flag = true;
        }
        std::map<std::string, int>().swap(changed_params);
#if debug_show_cam_params
        std::map<std::string, int>::reverse_iterator iter1;
        if (show_flag) {
            for (iter1 = original_params.rbegin(); iter1 != original_params.rend(); iter1++)
            {
                LOG(INFO) << iter1->first << " : " << iter1->second << std::endl;
            }
            show_flag = false;
        }
#endif
    }
}

//断开连接回调
void isConnected_callback() {

}

// 批处理回调函数
void Encoder_onBatchDataCallCack_147(const void* info, const AIeveR_Data* data)
{
    //如果达到需要回调的次数，则不执行后续的代码
    if (g_callBackCount_0.load() == g_needCallbackCount_)
    {
        return;
    }
    // 每条线的点的数量 例如( L10400 : data_width = 3200)
    int data_width_ = data->data_width;


    // 当前批次数据的行数
    int lineNums = data->encoder_value_vec.size();

    // 手动申请下面轮廓数据解析需要用的内存空间
    std::vector<AIeveR_Point3F> PC_3200_VEC;

    // 获取到的所有点的数量（ = lineNums * data_width_）
    int pointNum = data->pc_ptr_length_;
    PC_3200_VEC.reserve(data->pc_ptr_length_);

    // 从相机获取到的整型数据解析为轮廓数据 (uint z -> float z)
    // 这里解析出来的数据只有z值不能用于后续的拼接。
    std::vector<float> z_vec; // （用于存储解析后Z值的容器）
    global_postProcessing_.DecodeProfilesZ(data->pc_ptr_, data->pc_ptr_length_,
        z_vec, data->pc_ptr_length_);

    // 从相机获取到的整型数据解析为轮廓数据 (uint z -> float xyz)
    // 这里解析出来的是三维数据，可用于后续的拼接操作。
    global_postProcessing_.DecodeProfilesXYZ(data->pc_ptr_, data->pc_ptr_length_,
        PC_3200_VEC, data->pc_ptr_length_);

    // 这里将解析后所有的轮廓数据装入容器ALL_PC_VEC。
    test_147.ALL_PC_VEC_.insert(test_147.ALL_PC_VEC_.end(), PC_3200_VEC.begin(), PC_3200_VEC.end());


    // 将每行数据对应的编码器值装入容器ENCODER_VEC
    test_147.ENCODER_VEC_.insert(test_147.ENCODER_VEC_.end(), data->encoder_value_vec.begin(),
        data->encoder_value_vec.end());


    // 将每行数据对应的帧号值装入容器FRAME_VEC
    test_147.FRAME_VEC_.insert(test_147.FRAME_VEC_.end(), data->frame_cnt_vec.begin(),
        data->frame_cnt_vec.end());

    //灰度数据保存
    if (data->gray_ptr_length_ > 0)
    {
        test_147.ALL_GRAY_VEC_.insert(test_147.ALL_GRAY_VEC_.end(), data->gray_ptr_, data->gray_ptr_ + data->gray_ptr_length_);
    }

    //global_postProcessing_.resetProfileStitcher();
    //// 设置每个编码器脉冲之间的距离
    //// 代表设定的是编码器脉冲之间的距离值。
    //LOG(INFO) << "in Encoder_Handle_Data - move_vec(x|y|z): " << scan_move_vec_[0]->x << " | " << scan_move_vec_[0]->y << " | " << scan_move_vec_[0]->z << "\n";
    //AIeveR_Point3D mov_vec(scan_move_vec_[0]->x, scan_move_vec_[0]->y, scan_move_vec_[0]->z);
    //global_postProcessing_.setDistInterval(profile_stitch_dist[0], true);
    ////设置拼接方向向量
    //global_postProcessing_.setMoveDirection(mov_vec);
    //// 构造用于拼接的结构体
    //PostProcessing::ProfileStitcherParams tmp_ProfileStitcherParams;
    //// 传入需要拼接的点云数据
    //tmp_ProfileStitcherParams.PointVec = PC_3200_VEC;
    //// 传入点云数据对应的编码器值
    //tmp_ProfileStitcherParams.FlagValues = data->encoder_value_vec;
    //// 开始拼接，并指定为编码器值拼接的方式。
    //ErrorStatus stitch_status = global_postProcessing_.ProfileStitch(tmp_ProfileStitcherParams, true);
    //// 得到拼接后的轮廓数据:tmp_ProfileStitcherParams.PointVec;
    ////保存所有数据

    //    // 这里将解析后所有的轮廓数据装入容器ALL_PC_VEC。
    //test_147.ALL_PC_VEC_.insert(test_147.ALL_PC_VEC_.end(), tmp_ProfileStitcherParams.PointVec.begin(), tmp_ProfileStitcherParams.PointVec.end());


    //// 将每行数据对应的编码器值装入容器ENCODER_VEC
    //test_147.ENCODER_VEC_.insert(test_147.ENCODER_VEC_.end(), tmp_ProfileStitcherParams.FlagValues.begin(),
    //    tmp_ProfileStitcherParams.FlagValues.end());


    //// 将每行数据对应的帧号值装入容器FRAME_VEC
    //test_147.FRAME_VEC_.insert(test_147.FRAME_VEC_.end(), data->frame_cnt_vec.begin(),
    //    data->frame_cnt_vec.end());

    ////灰度数据保存
    //if (data->gray_ptr_length_ > 0)
    //{
    //    test_147.ALL_GRAY_VEC_.insert(test_147.ALL_GRAY_VEC_.end(), data->gray_ptr_, data->gray_ptr_ + data->gray_ptr_length_);
    //}

    // 统计回调的次数
    g_callBackCount_0.fetch_add(1);
    // LOG(INFO) << "g_callBackCount_0: " << g_callBackCount_0 << "\n";
    return;

}

int ScannerLApi::Init() {
    // release_scanner_l_ptr();

    std::vector<std::string> (SCANNER_CONFIG_FILE_VEC.size(), "").swap(scanner_l_ipv4_vec_);
    std::vector<std::vector<int>> (SCANNER_CONFIG_FILE_VEC.size(), std::vector<int> (2, -1)).swap(scanner_l_zrange_vec_);
    // std::vector<std::vector<int>> (SCANNER_CONFIG_FILE_VEC.size(), std::vector<int> (2, -1)).swap(scanner_l_zrange_vec_);
    //设置host
    for (int i = 0; i < scanner_l_ipv4_vec_.size(); i++) {
        scanner_l_config_vec_.push_back(new AIeveR_HostInfo());
    }
    //设置相机
    for (int i = 0; i < scanner_l_ipv4_vec_.size(); i++) {
        scanner_l_info_.push_back(new AIeveR_ScannerInfo());
    }
    //相机功能，connect等
    std::vector<AIScannerDeveloper*> (scanner_l_ipv4_vec_.size(), nullptr).swap(scanner_l_ptr_vec_);
    for (int i = 0; i < scanner_l_ipv4_vec_.size(); i++) {
        scanner_l_ptr_vec_[i] = new AIScannerDeveloper();
    }
    //MOVE_DIRECTION 参数
    std::vector<AIeveR_Point3D*> (scanner_l_ipv4_vec_.size(), nullptr).swap(scanner_l_move_vec_);
    std::vector<AIeveR_Point3D*>(scanner_l_ipv4_vec_.size(), nullptr).swap(scan_move_vec_);
    for (int i = 0; i < scanner_l_ipv4_vec_.size(); i++) {
        scanner_l_move_vec_[i] = new AIeveR_Point3D();
        scan_move_vec_[i] = new AIeveR_Point3D();
    }

    // 搜索在线相机方式
    // ...

    // READ config(Device info, camera config, RT transform) from Json
    for (int i = 0; i < SCANNER_CONFIG_FILE_VEC.size(); i++) {
        cv::Mat multi_calib_rt;

        bool be_main_scan = false;
        double profile_dist = 0.0;
        int callback_cnt = 0;
        int flag_load = load_config_file(config_root_path_, SCANNER_CONFIG_FILE_VEC[i], *scanner_l_config_vec_[i], *scanner_l_info_[i], scanner_l_zrange_vec_[i], multi_calib_rt, be_main_scan, profile_dist, callback_cnt, *scanner_l_move_vec_[i], *scan_move_vec_[i]);
        LOG(INFO) << "flag_load: " << flag_load << "\n";
        if (flag_load != 0)
            return -2;

        multi_calib_rt.convertTo(multi_calib_rt, CV_64FC1);
        scanner_l_rt_vec_.emplace_back(multi_calib_rt.clone());
        profile_stitch_distances.push_back(profile_dist);
        g_needCallbackCount_ = callback_cnt;

        if (be_main_scan)
            main_scan_index_ = i;
    }

    AIeveR_HostInfo host_info;
    for(int i = 0; i < scanner_l_ipv4_vec_.size();i++){
        host_info.Host_IP = scanner_l_config_vec_[i]->Host_IP;
        host_info.Host_Port = scanner_l_config_vec_[i]->Host_Port;
        // std::cout << "host_info.Host_IP " << i << ": " << host_info.Host_IP << "\n";
        // std::cout << "host_info.Host_Port " << i << ": " << host_info.Host_Port << "\n";
        
        //
        ErrorStatus sethost_error = scanner_l_ptr_vec_[i]->setHostInfo(host_info);//*scanner_l_config_vec_[i]
        // std::cout << "sethost_error errorCode: " << sethost_error.errorCode << "\n";
        // std::cout << "sethost_error errorDescription: " << sethost_error.errorDescription << "\n";
        if(sethost_error.isOK()){
            LOG(INFO) << "Init scanner successful!";
        }
        else{
            LOG(ERROR) << "Init scanner failed!";
            return -1;
        }
    }

    AIeveR_ScannerInfo scanner_info;
    for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        ErrorStatus connect_status = scanner_l_ptr_vec_[i]->registerDisconnectEventCallback(isConnected_callback);
        LOG(INFO) << "before connect test: " << connect_status.errorCode;
        LOG(INFO) << "before connect test: " << connect_status.errorDescription;
        if (connect_status.isOK()) {
            LOG(INFO) << "Connect :" << scanner_info.Scanner_Ip << " on";
        }
    }

    //连接相机
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        unsigned int max_try_cnt = 30;
        unsigned int try_cnt = 0;
        scanner_info.Scanner_Ip = scanner_l_info_[i]->Scanner_Ip;
        scanner_info.Scanner_Mac = scanner_l_info_[i]->Scanner_Mac;
        scanner_info.Scanner_Port = scanner_l_info_[i]->Scanner_Port;
        scanner_info.Scanner_Type = scanner_l_info_[i]->Scanner_Type;
        scanner_info.Working_Distance = scanner_l_info_[i]->Working_Distance;
        while (try_cnt < max_try_cnt) {
            ErrorStatus connect_status = scanner_l_ptr_vec_[i]->connect(scanner_info);
            LOG(INFO) << "connect_status: " << connect_status.isOK();
            LOG(INFO) << "errorCode: " << connect_status.errorCode << "\n";
            LOG(INFO) << "errorDescription: " << connect_status.errorDescription << "\n";
            if (connect_status.isOK()) {
                LOG(INFO) << "Connect :" << scanner_info.Scanner_Ip << " sucessfully";
                break;
            }
            else{
                LOG(ERROR) << "Connect :" << scanner_info.Scanner_Ip << " fail";
                try_cnt++;
                continue;
            }
            _sleep(500);
        }
        if (try_cnt >= max_try_cnt)
            return -1;
    }
    for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        ErrorStatus connect_status = scanner_l_ptr_vec_[i]->registerDisconnectEventCallback(isConnected_callback);
        LOG(INFO) << "test: " << connect_status.errorCode;
        LOG(INFO) << "test: " << connect_status.errorDescription;
        if (connect_status.isOK()) {
            LOG(INFO) << "Connect :" << scanner_info.Scanner_Ip << " on";
        }
    }
    //参数设置相关
    for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        std::string scannerIP_path = scanner_param_path;
        LOG(INFO) << "scannerIP_path: " << scannerIP_path << "\n";

        //如果连接成功，开始下发参数
        ErrorStatus load_status = scanner_l_ptr_vec_[i]->loadParameters(scannerIP_path);
        LOG(INFO) << "load_status: " << load_status.isOK();
        LOG(INFO) << "errorCode: " << load_status.errorCode << "\n";
        LOG(INFO) << "errorDescription: " << load_status.errorDescription << "\n";
        //成功配置相机默认参数
        if (load_status.isOK())
        {
            LOG(INFO) << SCANNER_CONFIG_FILE_TXT[i] << " Scanner Params Success.";
        }
        else {
            LOG(ERROR) << SCANNER_CONFIG_FILE_TXT[i] << "Scanner Params fail.";
            return -1;
        }
        ErrorStatus set_status = scanner_l_ptr_vec_[i]->getParameterValue(Scanner_Setting::BatchDataValue::name, batch_value);
        LOG(INFO) << "batch_value: " << batch_value;
        set_status = scanner_l_ptr_vec_[i]->getParameterValue(Scanner_Setting::LaserInten::name, setLaserIntense);
        LOG(INFO) << "scanner_info.Scanner_Ip: " << scanner_info.Scanner_Ip << " is ok ? " << set_status.isOK() << "\n";
        LOG(INFO) << "scanner_info.Scanner_Ip: " << scanner_info.Scanner_Ip << " get laser intense: " << setLaserIntense << "\n";
    }

#if dynamic_change_cam_params
    load_params(scanner_param_path, original_params);
    LOG(INFO) << "load_params :" << scanner_param_path;

    std::thread reload_cam_params(&ScannerLApi::camera_params_load, this);
    reload_cam_params.detach();
#endif

    //断开连接
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        scanner_l_ptr_vec_[i]->stop();
        scanner_l_ptr_vec_[i]->getCameraInfo(scanner_info);
        LOG(INFO) << "Stop scanner: " << scanner_info.Scanner_Ip << "\n";
        scanner_l_ptr_vec_[i]->disconnect();
        LOG(INFO) << "disconnect scanner: " << scanner_info.Scanner_Ip << "\n";
    }
    
    for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        ErrorStatus connect_status = scanner_l_ptr_vec_[i]->registerDisconnectEventCallback(isConnected_callback);
        LOG(INFO) << "test1: " << connect_status.errorCode;
        LOG(INFO) << "test1: " << connect_status.errorDescription;
        if (connect_status.isOK()) {
            LOG(INFO) << "Connect :" << scanner_info.Scanner_Ip << " off";
        }
    }
    return 0;
}


void ScannerLApi::Encoder_Handle_Data(Scanner_All_Data& all_data, const std::string& scanner_num, AIeveR_Point3D& mv_vec)
{  
    std::vector<uint32_t> frame_cnt_vec;
    std::vector<int32_t> encoder_vec;
    // 重置后处理类的状态
    global_postProcessing_.resetProfileStitcher();
    // 设置每个编码器脉冲之间的距离
    // 代表设定的是编码器脉冲之间的距离值。
    LOG(INFO) << "in Encoder_Handle_Data - move_vec(x|y|z): " << mv_vec.x << " | " << mv_vec.y << " | " << mv_vec.z << "\n";
    global_postProcessing_.setDistInterval(profile_stitch_distances[0], true);
    //设置拼接方向向量
    global_postProcessing_.setMoveDirection(mv_vec);
    // 构造用于拼接的结构体
    PostProcessing::ProfileStitcherParams tmp_ProfileStitcherParams;
    // 传入需要拼接的点云数据
    tmp_ProfileStitcherParams.PointVec = all_data.ALL_PC_VEC_;
    // 传入点云数据对应的编码器值
    tmp_ProfileStitcherParams.FlagValues = all_data.ENCODER_VEC_;
    // 开始拼接，并指定为编码器值拼接的方式。
    ErrorStatus stitch_status = global_postProcessing_.ProfileStitch(tmp_ProfileStitcherParams, true);
    LOG(INFO) << "scanner " << scanner_num << " stitch_status: " << stitch_status.isOK() << "\n";
    // 得到拼接后的轮廓数据:tmp_ProfileStitcherParams.PointVec;
    //保存所有数据
    all_data.ALL_PC_VEC_SAVE = tmp_ProfileStitcherParams.PointVec;
    all_data.ENCODER_VEC_SAVE = tmp_ProfileStitcherParams.FlagValues;
    all_data.ALL_GRAY_VEC_SAVE = all_data.ALL_GRAY_VEC_;
    all_data.FRAME_VEC_SAVE = all_data.FRAME_VEC_;

    ////x轴的行encoder值转点encoder值
    //for(int i = 0; i < all_data.ENCODER_VEC_SAVE.size(); i++){
    //    for(int j = 0; j < 3200; j++){
    //        encoder_vec.push_back(all_data.ENCODER_VEC_SAVE[i]);
    //    }
    //}
    ////x轴的行frame值转点frame值
    //for(int i = 0; i < all_data.FRAME_VEC_SAVE.size(); i++){
    //    for(int j = 0; j < 3200; j++){
    //        frame_cnt_vec.push_back(all_data.FRAME_VEC_SAVE[i]);
    //    }
    //}
    
    /*int data_width = 0;
    scanner_l_ptr_vec_[0]->getDataWidth(data_width);
    for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        std::string path_laser_scan_pc = "./pointclouds_loop_scan_" + std::to_string(i) + "_gray.tiff";
        LOG(INFO) << "save gray tiff to: " << path_laser_scan_pc;
        AIScannerDeveloper::saveGrayToTIFF(all_data.ALL_GRAY_VEC_SAVE, data_width, path_laser_scan_pc);
    }*/

    // std::string pc_save_file_path = "./ProfilePointCloud_EncoderCallback_" + scanner_num + ".ply";
    // AIScannerDeveloper::savePointCloudToPly(tmp_ProfileStitcherParams.PointVec, all_data.ALL_GRAY_VEC_, encoder_vec, frame_cnt_vec, pc_save_file_path, PlyFileFormat::BINARY, true);

}

int ScannerLApi::Connect() {
    //连接相机
    auto connect_time = std::chrono::system_clock::now();
    AIeveR_ScannerInfo scanner_info;
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        unsigned int max_try_cnt = 30;
        unsigned int try_cnt = 0;
        scanner_info.Scanner_Ip = scanner_l_info_[i]->Scanner_Ip;
        scanner_info.Scanner_Mac = scanner_l_info_[i]->Scanner_Mac;
        scanner_info.Scanner_Port = scanner_l_info_[i]->Scanner_Port;
        scanner_info.Scanner_Type = scanner_l_info_[i]->Scanner_Type;
        scanner_info.Working_Distance = scanner_l_info_[i]->Working_Distance;

        while (try_cnt < max_try_cnt) {
            ErrorStatus connect_status = scanner_l_ptr_vec_[i]->connect(scanner_info);
            LOG(INFO) << "connect_status: " << connect_status.isOK();
            LOG(INFO) << "errorCode: " << connect_status.errorCode << "\n";
            LOG(INFO) << "errorDescription: " << connect_status.errorDescription << "\n";
            if (connect_status.isOK()) {
                LOG(INFO) << "Connect :" << scanner_info.Scanner_Ip << " sucessfully";
                break;
            }
            else{
                LOG(ERROR) << "Connect :" << scanner_info.Scanner_Ip << " fail";
                try_cnt++;
                continue;
            }
            _sleep(500);
        }
        if (try_cnt >= max_try_cnt)
            return -1;
    }
    /*for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        ErrorStatus connect_status = scanner_l_ptr_vec_[i]->registerDisconnectEventCallback();
    }*/


    auto connect_time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - connect_time).count();
    LOG(INFO) << "connect_time_diff: " << connect_time_diff << " ms\n";
    //回调注册相关
    auto reg_callback_time = std::chrono::system_clock::now();
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        scanner_l_ptr_vec_[i]->getCameraInfo(scanner_info);
        //注册  
        ErrorStatus set_status = scanner_l_ptr_vec_[i]->setBatchDataHandler(Encoder_onBatchDataCallCack_147, batch_value);
        LOG(INFO) << scanner_info.Scanner_Ip << ":bind recall function Encoder_onBatchDataCallCack_147:" << set_status.isOK();
    }
    auto reg_callback_time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - reg_callback_time).count();
    LOG(INFO) << "reg_callback_time_diff: " << reg_callback_time_diff << " ms\n";
    return 0;
}

int ScannerLApi::Reset() {
    // g_callBackCount_0 = 0;
    // g_callBackCount_1 = 0;
    return 0;
}

void callback_go(){

    while (thr_flag)
    {
        int total_pgbar = g_needCallbackCount_;
        int barWidth_pgbar = 50;
        while(1)
        {
            //printProgressBar(g_callBackCount_0, total_pgbar, barWidth_pgbar);

            if (!thr_flag) {
                LOG(INFO) << "callback time not enough but Recall switch turn off\n";
                LOG(INFO) << "thr_flag : " << thr_flag << "\n";
                break;
            }

            if (g_callBackCount_0.load() == g_needCallbackCount_)
            {    
                LOG(INFO) << "Recall switch turn off\n";
                thr_flag.store(false);
                LOG(INFO) << "thr_flag : " << thr_flag << "\n";
                break;
            }
            //_sleep(10);
        }
    }
}


int ScannerLApi::Start() {
    auto swap_time = std::chrono::system_clock::now();
    // 147
    std::vector<uint8_t>().swap(test_147.ALL_GRAY_VEC_);std::vector<AIeveR_Point3F>().swap(test_147.ALL_PC_VEC_);
    std::vector<uint32_t>().swap(test_147.FRAME_VEC_);std::vector<int32_t>().swap(test_147.ENCODER_VEC_);
    std::vector<uint8_t>().swap(test_147.ALL_GRAY_VEC_SAVE);std::vector<AIeveR_Point3F>().swap(test_147.ALL_PC_VEC_SAVE);
    std::vector<uint32_t>().swap(test_147.FRAME_VEC_SAVE);std::vector<int32_t>().swap(test_147.ENCODER_VEC_SAVE);
    //all data
    std::vector<Scanner_All_Data>().swap(all_PC_data);

    g_callBackCount_0.store(0);
    auto swap_time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - swap_time).count();
    LOG(INFO) << "swap_time_diff: " << swap_time_diff << " ms\n";
    //开启批处理
    std::vector<ErrorStatus> start_status;
    start_status.resize(scanner_l_ptr_vec_.size());
    AIeveR_ScannerInfo scanner_info;
    // std::thread start_threads[4];

    auto start_time = std::chrono::system_clock::now();
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        scanner_l_ptr_vec_[i]->getCameraInfo(scanner_info);
        LOG(INFO) << "scanner_info.Scanner_Ip: " << scanner_info.Scanner_Ip << "\n";

        //start
        // start_threads[i] = std::thread(scanner_l_ptr_vec_[i]->start());
        start_status.push_back(scanner_l_ptr_vec_[i]->start());
        LOG(INFO) << "start_status: " << scanner_info.Scanner_Ip << " : " << start_status[i].isOK();

    }

    // int total_pgbar = g_needCallbackCount_;
    // int barWidth_pgbar = 50;
    if (start_status[0].isOK())
    {
        thr_flag.store(true);
        std::thread getdatathr(callback_go);
        getdatathr.detach();

        for(int i = 0; i < scanner_l_ptr_vec_.size() ;i++){
            scanner_l_ptr_vec_[i]->getCameraInfo(scanner_info);
            // set_status = scanner_l_ptr_vec_[i]->getParameterValue(Scanner_Setting::LaserInten::name, setLaserIntense);
            //打开激光
            LOG(INFO) << "scanner_info.Scanner_Ip: " << scanner_info.Scanner_Ip << "set laser intense: " << setLaserIntense << "\n";
            ErrorStatus set_status = scanner_l_ptr_vec_[i]->setParameterValue(Scanner_Setting::LaserInten::name, setLaserIntense);
            //打开批处理
            set_status = scanner_l_ptr_vec_[i]->setParameterValue(Scanner_Setting::BatchDataCallBackSwitch::name, true);
            LOG(INFO) << "scanner_info.Scanner_Ip: " << scanner_info.Scanner_Ip << "open laser and turn on recall switch\n";
        }
    }
    else{
        LOG(INFO) << "start scanner failed!";
        return -1;
    }
    auto start_time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count();
    LOG(INFO) << "start_time_diff: " << start_time_diff << " ms\n";
    return 0;
}

int ScannerLApi::End() {
    thr_flag.store(false);
    AIeveR_ScannerInfo scanner_info;
    //停止采集
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        scanner_l_ptr_vec_[i]->setParameterValue(Scanner_Setting::BatchDataCallBackSwitch::name, false);
        //关闭激光
        ErrorStatus set_status = scanner_l_ptr_vec_[i]->setParameterValue(Scanner_Setting::LaserInten::name, 0);
        scanner_l_ptr_vec_[i]->stop();
        scanner_l_ptr_vec_[i]->getCameraInfo(scanner_info);
        LOG(INFO) << "Stop scanner: " << scanner_info.Scanner_Ip << "\n";
    }
    LOG(INFO) << "********test_147 size********";
    LOG(INFO) << "test_147.ALL_GRAY_VEC_.size(): " << test_147.ALL_GRAY_VEC_.size();
    LOG(INFO) << "test_147.ALL_GRAY_VEC_SAVE.size(): " << test_147.ALL_GRAY_VEC_SAVE.size();
    LOG(INFO) << "test_147.ALL_PC_VEC_.size(): " << test_147.ALL_PC_VEC_.size();
    LOG(INFO) << "test_147.ALL_PC_VEC_SAVE.size(): " << test_147.ALL_PC_VEC_SAVE.size();
    // 处理获取到的数据
    all_PC_data.push_back(test_147);
    AIeveR_Point3D mv_vec_;
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        std::string scanner_num = std::to_string(i);
        mv_vec_.x = scanner_l_move_vec_[i]->x;
        mv_vec_.y = scanner_l_move_vec_[i]->y;
        mv_vec_.z = scanner_l_move_vec_[i]->z;
        LOG(INFO) << "call Encoder_Handle_Data - move_vec(x|y|z): " << mv_vec_.x << " | " << mv_vec_.y << " | " << mv_vec_.z << "\n";
        Encoder_Handle_Data(all_PC_data[i], scanner_num, mv_vec_);
        scanner_l_ptr_vec_[i]->getCameraInfo(scanner_info);
        LOG(INFO) << "Get data from scanner: " << scanner_info.Scanner_Ip << "\n";

    }
    LOG(INFO) << "********all_PC_data[0] size********";
    LOG(INFO) << "all_PC_data[0].ALL_GRAY_VEC_.size(): " << all_PC_data[0].ALL_GRAY_VEC_.size();
    LOG(INFO) << "all_PC_data[0].ALL_GRAY_VEC_SAVE.size(): " << all_PC_data[0].ALL_GRAY_VEC_SAVE.size();
    LOG(INFO) << "all_PC_data[0].ALL_PC_VEC_.size(): " << all_PC_data[0].ALL_PC_VEC_.size();
    LOG(INFO) << "all_PC_data[0].ALL_PC_VEC_SAVE.size(): " << all_PC_data[0].ALL_PC_VEC_SAVE.size();
    return 0;
}


int ScannerLApi::disconnect(){
    //disconnect重要
    for(int i = 0; i < scanner_l_ptr_vec_.size();i++){
        scanner_l_ptr_vec_[i]->disconnect();
    }
    return 0;
}

int ScannerLApi::GetAllData(std::vector<std::vector<cv::Point3f>>& out_pc_vec, 
                            std::vector<std::vector<uint8_t>>& out_gray_vec,
                            std::vector<std::vector<int32_t>>& out_encoder_vec,
                            std::vector<std::vector<uint32_t>>& out_framecnt_vec) {

    std::vector<std::vector<cv::Point3f>> (SCANNER_CONFIG_FILE_TXT.size(), std::vector<cv::Point3f>()).swap(out_pc_vec);
    std::vector<std::vector<uint8_t>> (SCANNER_CONFIG_FILE_TXT.size(), std::vector<uint8_t>()).swap(out_gray_vec);
    std::vector<std::vector<int32_t>> (SCANNER_CONFIG_FILE_TXT.size(), std::vector<int32_t>()).swap(out_encoder_vec);
    std::vector<std::vector<uint32_t>> (SCANNER_CONFIG_FILE_TXT.size(), std::vector<uint32_t>()).swap(out_framecnt_vec);
    out_pc_vec.resize(SCANNER_CONFIG_FILE_TXT.size());
    out_gray_vec.resize(SCANNER_CONFIG_FILE_TXT.size());
    out_encoder_vec.resize(SCANNER_CONFIG_FILE_TXT.size());
    out_framecnt_vec.resize(SCANNER_CONFIG_FILE_TXT.size());
    std::vector<uint32_t> frame_cnt_vec;
    std::vector<int32_t> encoder_vec;
    std::vector<cv::Point3f> frame_pc_vec;
    for (int cam = 0; cam < SCANNER_CONFIG_FILE_TXT.size(); cam++) {

        //x轴的行encoder值转点encoder值
        for(int i = 0; i < all_PC_data[cam].ENCODER_VEC_SAVE.size(); i++){
            for(int j = 0; j < 3200; j++){
                encoder_vec.push_back(all_PC_data[cam].ENCODER_VEC_SAVE[i]);
            }
        }
        //x轴的行frame值转点frame值
        for(int i = 0; i < all_PC_data[cam].FRAME_VEC_SAVE.size(); i++){
            for(int j = 0; j < 3200; j++){
                frame_cnt_vec.push_back(all_PC_data[cam].FRAME_VEC_SAVE[i]);
            }
        }
        //转opencv格式
        for(int i = 0;i < all_PC_data[cam].ALL_PC_VEC_SAVE.size();i++){
            cv::Point3f p(0, 0, 0);
            p.x = all_PC_data[cam].ALL_PC_VEC_SAVE[i].x;
            p.y = all_PC_data[cam].ALL_PC_VEC_SAVE[i].y;
            p.z = all_PC_data[cam].ALL_PC_VEC_SAVE[i].z;
            frame_pc_vec.push_back(p);
        }

        // TODO:
        // RT transform and then move calibration offset.
        // cv::Mat T_mat =  global_transform_mat_ * scanner_l_rt_vec_[cam];//cv::Mat::eye(4,4,CV_64FC1);

        int32_t encoder_s = encoder_vec[0];
        int32_t prev_encoder = encoder_s;
        int32_t encoder_flip = 0;

        for (int j = 0; j < frame_pc_vec.size(); j++) {
            int32_t i_encoder_val = encoder_vec[j];
            uint32_t i_frame_cnt_val = frame_cnt_vec[j];
            // if (i_encoder_val > 65535)
            //     continue;

            // if (i_encoder_val < prev_encoder) {
            //     encoder_flip++;
            // }

            // prev_encoder = i_encoder_val;

            // float mv_dist = (i_encoder_val + encoder_flip * 65535 - encoder_s) * platfrom_offset.dist_per_pluse_y_;

            //变换坐标系，从相机 -> 主相机
            // for (int k = 0; k < frame_pc_vec.size(); k++) {

            cv::Point3f p_org = frame_pc_vec[j];
            //过滤噪声点
            //if (p_org.z < scanner_l_zrange_vec_[0][0] || p_org.z > scanner_l_zrange_vec_[0][1]) continue;

            // cv::Point3f p(0, 0, 0);
            // // [R T][x y z 1]^T
            // p.x = p_org.x;
            // p.y = p_org.x;
            // p.z = p_org.x;
            //运动补偿
            // p.x += mv_dist * platfrom_offset.moveplatform_offset_factor_x_;
            // p.y += mv_dist * platfrom_offset.moveplatform_offset_factor_y_;
            // p.z += mv_dist * platfrom_offset.moveplatform_offset_factor_z_;

            out_pc_vec[cam].push_back(p_org);
            out_gray_vec[cam].push_back(all_PC_data[cam].ALL_GRAY_VEC_SAVE[j]);

            out_encoder_vec[cam].push_back(i_encoder_val);//relative_encoder_val
            out_framecnt_vec[cam].push_back(i_frame_cnt_val);

        }
        frame_cnt_vec.clear();
        encoder_vec.clear();
        frame_pc_vec.clear();
    }
    LOG(INFO) << "********out vec[0] size********";
    LOG(INFO) << "out_pc_vec[0].size(): " << out_pc_vec[0].size();
    LOG(INFO) << "out_gray_vec[0].size(): " << out_gray_vec[0].size();
    return 0;
}



/*----------------- Private -------------------*/
int ScannerLApi::load_config_file(std::string config_rootpath, std::string config_filename , AIeveR_HostInfo& out_scanner_config, AIeveR_ScannerInfo& out_scanner_l_info, std::vector<int>& out_zrange, cv::Mat& out_multi_calib_rt, bool& main_scan, double& profile_stitch_dist, int& callback_cnt, AIeveR_Point3D& out_scanner_l_mv_vec, AIeveR_Point3D& scan_mov_vec) {
    std::string json_file_abs_path = config_rootpath + config_filename;
    std::ifstream f(json_file_abs_path);

    LOG(INFO) << "json file abs path: " << json_file_abs_path << "\n";
    if (!f.is_open()) {
        LOG(INFO) << "ERROR - Fail to open json file " << json_file_abs_path;
        return -1;
    }
    json data = json::parse(f);
    f.close();

    out_scanner_config.Host_IP = data["host_ip_v4_addr"];
    out_scanner_config.Host_Port = data["host_ip_v4_port"];

    out_scanner_l_info.Scanner_Ip = data["scanner_ip"];
    out_scanner_l_info.Scanner_Mac = data["scanner_mac"];
    out_scanner_l_info.Scanner_Port = data["scanner_port"];
    out_scanner_l_info.Scanner_Type = data["scanner_type"];
    out_scanner_l_info.Working_Distance = data["working_distance"];

    // platfrom_offset.dist_per_pluse_y_ = data["mp_distperpulse"];
    // platfrom_offset.moveplatform_offset_factor_x_ = data["mp_calib_factor_x"];
    // platfrom_offset.moveplatform_offset_factor_y_ = data["mp_calib_factor_y"];
    // platfrom_offset.moveplatform_offset_factor_z_ = data["mp_calib_factor_z"];
    out_scanner_l_mv_vec.x = data["mp_calib_factor_x"];
    out_scanner_l_mv_vec.y = data["mp_calib_factor_y"];
    out_scanner_l_mv_vec.z = data["mp_calib_factor_z"];

    scan_mov_vec.x = data["mp_calib_factor_x"];
    scan_mov_vec.y = data["mp_calib_factor_y"];
    scan_mov_vec.z = data["mp_calib_factor_z"];

    profile_stitch_dist = data["profile_stitch_distance"];
    callback_cnt = data["needCallbackCount"];

    main_scan = data["main_scan"];
    int zrange_low = data["zrange_low"];
    int zrange_high = data["zrange_high"];
    out_zrange[0] = zrange_low;
    out_zrange[1] = zrange_high;

    calibration_z_compensation_ = data["calibration_z_compensation"];

    backward_compensation_pt_ = cv::Point3f(data["backward_x_compensation"], 
                                            data["backward_y_compensation"], 
                                            data["backward_z_compensation"]);

    //读标定参数
    std::string multi_calib_config = data["multi_calib_config"];
    std::string batterytype_rt_path = data["batterytype_rt_path"];
#if 1
    cv::FileStorage fs_multi_calib(config_rootpath + multi_calib_config, cv::FileStorage::READ);

    LOG(INFO) << "fs_multi_calib path: " << config_rootpath + multi_calib_config << "\n";
    if (!fs_multi_calib.isOpened()) {
        LOG(INFO) << "ERROR - Failed to open file " << multi_calib_config;
        fs_multi_calib.release();
        return -2;
    }

    fs_multi_calib["multi_calib_rt"] >> out_multi_calib_rt;
    fs_multi_calib.release();
#endif
    return 0;
}

void ScannerLApi::release_scanner_l_ptr() {
    for (int i = 0; i < scanner_l_ptr_vec_.size(); i++) {
        if (scanner_l_ptr_vec_[i] == nullptr)
            continue;

        delete scanner_l_ptr_vec_[i];
        scanner_l_ptr_vec_[i] = nullptr;
    }
    for (int i = 0; i < scanner_l_info_.size(); i++) {
        if (scanner_l_info_[i] == nullptr)
            continue;

        delete scanner_l_info_[i];
        scanner_l_info_[i] = nullptr;
    }
    for (int i = 0; i < scanner_l_config_vec_.size(); i++) {
        if (scanner_l_config_vec_[i] == nullptr)
            continue;

        delete scanner_l_config_vec_[i];
        scanner_l_config_vec_[i] = nullptr;
    }

    for (int i = 0; i < scanner_l_rt_vec_.size(); i++) {
        scanner_l_rt_vec_[i].release();
    }
}
