#include "../include/scanner_l/FileWatcher.h"
#include "tchar.h"

int load_params(std::string filePath, std::map<std::string, int>& params_map) {
    std::ifstream inFile(filePath); // TXT文件路径
    if (!inFile) {
        LOG(ERROR) << "open file fail!\n";
    }
    std::string line;
    while (std::getline(inFile, line)) {
        // 找到 # 并删除其以后的内容
        size_t commentPos = line.find("#");
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        // 去除夹在左右空格的空白字符
        line.erase(0, line.find_first_not_of(" \t")); // 去除左空白
        line.erase(line.find_last_not_of(" \t") + 1); // 去除右空白

        // 只处理含有空格分割的行
        if (!line.empty() && line.find(" ") != std::string::npos) {
            std::vector<std::string> result;
            std::stringstream ss(line);
            std::string item;
            // 获取空格前后的参数名和参数值
            int count = 0;
            while (std::getline(ss, item, ' ')) {
                if (!item.empty()) {
                    result.push_back(item);
                }
                // 只要前两个
                if (++count == 2) {
                    break;
                }
            }

            if (result.size() == 2) {
                 //下发参数
               /* std::cout << result[0] << " | " << std::stoi(result[1]) << "\n";*/
                params_map.insert(std::pair<std::string, int>(result[0], std::stoi(result[1])));
            }
        }
    }
    inFile.close(); // 关闭文件

    /*std::map<std::string, int>::reverse_iterator iter1;
    for (iter1 = myMap.rbegin(); iter1 != myMap.rend(); iter1++)
    {
        std::cout << iter1->first << " : " << iter1->second << std::endl;
    }*/
    /*for (const auto& pair : myMap) {
        std::cout << "键: " << pair.first << ", 值: " << pair.second << std::endl;
    }*/
    return 0;
}

FileWatcher::FileWatcher(const std::string& file, unsigned int millis)
    : m_file(file)
    , m_waitMillis(millis)
    , m_stopped(false)
    , m_thread(&FileWatcher::run, this)
{
    m_lastFileInfo.mtime = time_cast(Clock::now ());
    m_lastFileInfo.size = 0;
 
    updateLastModInfo();
}
 
FileWatcher::~FileWatcher()
{
     m_stopped = true;
     if (m_thread.joinable())
     {
         m_thread.join();
     }
}
 
void FileWatcher::run()
{
    while (!m_stopped)
    {
        bool modified = checkForFileModification();
        if(modified)
        {
            updateLastModInfo();
            LOG(INFO) << "File changed" << std::endl;
            m_modifiedSign = true;

        }
 
        std::this_thread::sleep_for(std::chrono::milliseconds(m_waitMillis));
    }
}
 
bool FileWatcher::checkForFileModification()
{
    FileInfo fi;
 
    if (!getFileInfo(&fi, m_file) )
    {
        return false;
    }
 
    bool modified = fi.mtime > m_lastFileInfo.mtime
            || fi.size != m_lastFileInfo.size;
 
 
    return modified;
}
 
void FileWatcher::updateLastModInfo()
{
    FileInfo fi;
 
    if (getFileInfo(&fi, m_file))
    {
        m_lastFileInfo = fi;
    }
}
 
bool FileWatcher::getFileInfo(FileInfo *fi, const std::string &name)
{
    struct _stat fileStatus;
    if (_stat(name.c_str (), &fileStatus) == -1)
    {
        return false;
    }
 
    fi->mtime = from_time_t(fileStatus.st_mtime);
    fi->size = fileStatus.st_size;
 
    return true;
}

bool FileWatcher::getModifiedSign() {
    return m_modifiedSign;
}



void FileWatcher::setModifiedSign(bool sign) {
    m_modifiedSign = sign;
}