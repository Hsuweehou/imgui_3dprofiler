#ifndef MOTION_CONF_H
#define MOTION_CONF_H
#include <string>
#include <vector>
#include <nlohmann/json.hpp>


// X轴坐标信息, 单位mm
struct XCoordInfo
{

    // 待料位置
    float loading_pt = 0.0f;

    // 终点位置
    float line_scan_end_pt = 0.0f;

    XCoordInfo() = default;

};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(XCoordInfo, loading_pt, line_scan_end_pt)

/**
* @brief Y轴坐标信息, 单位mm
*
*/
struct YCoordInfo
{
    // 最高位
    float highest_pt = 0.0f;

    // 最低位
    float lowest_pt = 0.0f;

    //相机设计位
    float scanner_design_pt = 0.0f;
    YCoordInfo() = default;

};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(YCoordInfo, highest_pt, lowest_pt, scanner_design_pt)

/**
* @brief 配方信息
*
*/
struct Solution
{
    int id = -1;
    std::string name = "";
    int set_speed = 100;
    XCoordInfo x_coord_info;
    YCoordInfo y_coord_info;

};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Solution, id, name, x_coord_info, y_coord_info)


/**
 * @brief 运动控制配置信息
 *
 */
struct ConfigData {
    std::string plc_server_ip = "127.0.0.1";
    int plc_server_port = 502;
    int current_solution_id = -1;
    int moving_speed = 0;
    std::vector<Solution> solutions;

    bool LoadFromJson(const std::string& json_file_path);
    bool SaveToJson(const std::string& json_file_path);
    bool GetCurrentSolution(Solution& sol) const;
    bool SetCurrentSolution(int id);
    bool UpdateSolution(const Solution& sol);

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConfigData, current_solution_id, plc_server_ip, plc_server_port, moving_speed, solutions)
};

#endif /* MOTION_CONF_H */
