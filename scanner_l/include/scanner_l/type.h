#ifndef TYPE_H
#define TYPE_H

#include <iostream>

/**
 * @brief Ply 文件格式
 *
 */
enum class m_PlyFormat {
    ASCII = 0,  // default
    BINARY = 1
};

template <typename Key, typename T>
struct KeyData {
    Key key;
    T data;
    KeyData(Key key, T data) : key(key), data(data) {}
};

struct platfromOffset
{
    platfromOffset(){}

    ~platfromOffset(){
    }

    double dist_per_pluse_y_;
    double moveplatform_offset_factor_x_;
    double moveplatform_offset_factor_y_;
    double moveplatform_offset_factor_z_;
};


#endif