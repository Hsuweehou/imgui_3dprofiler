#ifndef FILEWATCHER_H
#define FILEWATCHER_H

#include <iostream>
#include <ctime>
#include <chrono>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <fstream>
#include <istream>
#include <sstream>
#include "glog/logging.h"

typedef std::chrono::system_clock Clock;
typedef std::chrono::duration<long long, std::micro> Duration;
typedef std::chrono::time_point<Clock, Duration> Time;
 
//比较两个map，返回不同键值
template<typename K, typename V>
std::vector<std::pair<K, V>> compare_maps(const std::map<K, V>& map1,
    const std::map<K, V>& map2) {
    std::vector<std::pair<K, V>> differences;

    // 检查map1中不在map2或值不同的元素
    for (const auto& pair : map1) {
        auto it = map2.find(pair.first);
        if (it == map2.end() || it->second != pair.second) {
            differences.push_back(pair);
        }
    }

    // 检查map2中不在map1的元素
    for (const auto& pair : map2) {
        if (map1.find(pair.first) == map1.end()) {
            differences.push_back(pair);
        }
    }
    return differences;
}

template<typename K, typename V>
std::vector<std::pair<K, V>> compare_maps1(const std::map<K, V>& map1,
    const std::map<K, V>& map2) {
    std::vector<std::pair<K, V>> differences;

    // 检查map1中有但map2中没有的键，或值不同的键
    for (const auto& pair : map1) {
        auto it = map2.find(pair.first);
        if (it == map2.end()) {
            differences.push_back(pair);
        }
        else if constexpr (std::is_floating_point_v<V>) {
            if (fabs(it->second - pair.second) > 0.000001) {
                differences.emplace_back(pair.first, pair.second);
            }
        }
        else {
            if (it->second != pair.second) {
                differences.emplace_back(pair.first, pair.second);
            }
        }
    }

    // 检查map2中有但map1中没有的键
    for (const auto& pair : map2) {
        if (map1.find(pair.first) == map1.end()) {
            differences.push_back(pair);
        }
    }

    return differences;
}

//加载参数
int load_params(std::string filePath, std::map<std::string, int>& params_map);

template <typename FromDuration>
inline Time time_cast (std::chrono::time_point<Clock, FromDuration> const & tp)
{
    return std::chrono::time_point_cast<Duration, Clock> (tp);
}
 
inline Time now ()
{
    return time_cast(Clock::now ());
}
 
inline Time from_time_t (time_t t_time)
{
    return time_cast(Clock::from_time_t (t_time));
}
 
struct FileInfo
{
    Time mtime;
    off_t size;
};
 
class FileWatcher
{
public:
    FileWatcher(const std::string& file, unsigned int millis);
    ~FileWatcher();
 
    bool getModifiedSign();
    void setModifiedSign(bool sign);

protected:
    void run();
 
private:
    bool checkForFileModification();
    void updateLastModInfo();
    bool getFileInfo(FileInfo *fi, const std::string &name);
 
private:
    FileInfo m_lastFileInfo;
    std::string m_file;
    unsigned int const m_waitMillis;
    std::atomic_bool m_stopped;
    std::thread m_thread;
    bool m_modifiedSign = false;
};
 
#endif // FILEWATCHER_H