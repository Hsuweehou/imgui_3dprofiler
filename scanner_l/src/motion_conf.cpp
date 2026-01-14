#include "../include/scanner_l/motion_conf.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <glog/logging.h>

//ConfigData parse_config(const std::string& filename) {
//    std::ifstream file(filename);
//    if (!file.is_open()) {
//        throw std::runtime_error("Failed to open config file");
//    }
//    return nlohmann::json::parse(file).get<ConfigData>();
//}
//
//MotionConfig::MotionConfig(std::string config_root): config_file_path(config_root)
//{
//    if(!LoadFromJson(config_root))
//    {
//        throw std::runtime_error("Failed to load motion config from json file: " + config_root);
//    }
//    std::ifstream file(config_root);
//    if (!file.is_open()) {
//        throw std::runtime_error("Failed to open config file");
//    }
//    return nlohmann::json::parse(file).get<ConfigData>();
//}

bool ConfigData::LoadFromJson(const std::string& json_file_path)
{
    std::ifstream config_file(json_file_path);
    if (!config_file.is_open())
    {
        LOG(ERROR) << "Failed to open config file: " << json_file_path;
        return false;
    }
    nlohmann::json config_json = nlohmann::json::parse(config_file);
    try
    {
		*this = config_json;
    }
    catch (const std::exception& ex)
    {
        LOG(ERROR) << ex.what();
        return false;
    }
    
    return true;
}

bool ConfigData::SaveToJson(const std::string &json_file_path)
{
    try
    {
        nlohmann::json json_root = *this;
        auto str = json_root.dump(4);
        std::ofstream out_file(json_file_path);
        if (out_file.is_open())
        {
            out_file << str;
        }
    }
    catch (const std::exception& ex)
    {
        LOG(ERROR) << ex.what();
        return false;
    }
    
}

bool ConfigData::GetCurrentSolution(Solution& sol) const
{
    for (auto solution : solutions)
    {
        if (solution.id == current_solution_id)
        {
            sol = solution;
            return true;
        } 
    }
    return false;
}

bool ConfigData::SetCurrentSolution(int id)
{
    for (auto solution : solutions)
    {
        if (solution.id == id)
        {
            current_solution_id = id;
        }
    }
    if (current_solution_id == id)
    {
        return true;
    }
    return false;
}

bool ConfigData::UpdateSolution(const Solution &sol)
{
    for (auto& solution : solutions)
    {
        if (solution.id == sol.id)
        {
            solution = sol;
            return true;
        }
    }
    solutions.push_back(sol);
    return true;
}
