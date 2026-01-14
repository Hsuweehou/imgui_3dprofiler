#ifndef DATA_CALLBACK_H
#define DATA_CALLBACK_H

#include "../../SDK_C++/include/AIeveR.h"
#include "scanner_all_data.h"

using namespace AIeveR::Device;
using namespace AIeveR::Algorithm;

class Data_Callback {
public:
    Data_Callback(int work_dist, int needed_num) : m_scanner_work_distance(work_dist),
                                                    m_needCallbackCount_(needed_num){};

    ~Data_Callback(){};

    void encoder_onBatchDataCallCack(const void *info, const AIeveR::Device::AIeveR_Data *data)
    {
        PostProcessing global_postProcessing_(m_scanner_work_distance);
        //如果达到需要回调的次数，则不执行后续的代码
        if (m_callBackCount == m_needCallbackCount_)
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
        m_save_data.ALL_PC_VEC_.insert(m_save_data.ALL_PC_VEC_.end(), PC_3200_VEC.begin(), PC_3200_VEC.end());


        // 将每行数据对应的编码器值装入容器ENCODER_VEC
        m_save_data.ENCODER_VEC_.insert(m_save_data.ENCODER_VEC_.end(), data->encoder_value_vec.begin(),
                            data->encoder_value_vec.end());


        // 将每行数据对应的帧号值装入容器FRAME_VEC
        m_save_data.FRAME_VEC_.insert(m_save_data.FRAME_VEC_.end(), data->frame_cnt_vec.begin(),
                            data->frame_cnt_vec.end());

        //灰度数据保存
        if (data->gray_ptr_length_ > 0)
        {
            m_save_data.ALL_GRAY_VEC_.insert(m_save_data.ALL_GRAY_VEC_.end(), data->gray_ptr_,data->gray_ptr_+ data->gray_ptr_length_);
        }

        // 统计回调的次数
        m_callBackCount++;
        // LOG(INFO) << "m_callBackCount: " << m_callBackCount << "\n";
        return;
    }

    int GetCallbackCounts() { return m_callBackCount; }
    Scanner_All_Data GetScannerAllData() { return m_save_data;}

private:
    int m_callBackCount = 0;
    int m_needCallbackCount_ = 30;
    int m_scanner_work_distance;
    Scanner_All_Data m_save_data;

};



#endif