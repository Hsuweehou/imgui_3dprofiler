#include "MainWindow.h"

#include <iostream>
#include <vector>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void GetScreenSize(int& width, int& height)
{
    //屏幕数量
    int monitorCount;
    GLFWmonitor** pMonitor = glfwGetMonitors(&monitorCount);
    int screen_x, screen_y;
    for (int i = 0; i < monitorCount; i++)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(pMonitor[i]);
        //屏幕大小
        screen_x = mode->width;
        screen_y = mode->height;
    }
    width = screen_x;
    height = screen_y;
}

//防止程序多开
HANDLE g_hEvent; //定义一个句柄
int main()
{
    g_hEvent = CreateEventW(NULL, 0, 0, LPCWSTR("Hi"));
    SetEvent(g_hEvent);
    if (g_hEvent)
    {
        if (ERROR_ALREADY_EXISTS == GetLastError()) {
            //如果多开会退出
            return 0;
        }
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return -1;

    //获取屏幕尺寸
    int width, height;
    GetScreenSize(width, height);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    std::unique_ptr<MainWindow> win = std::make_unique<MainWindow>(height, width);
    return 0;
}