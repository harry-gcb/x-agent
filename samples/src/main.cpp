#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

std::atomic<bool> running(true);

// 信号处理函数，用于优雅地退出程序
void signal_handler(int signum) {
    running = false;
}

double task(double x) {
    return std::sqrt(x);
}

// 模拟浮点计算的函数
void cpu_intensive_task(int count) {
    double x = 0.0001;
    for (int i = 0; i < count; ++i) {  
        x = task(x); // 复杂一些的浮点运算
    }
}

void worker() {
    const double PI = 3.14159265358979323846;
    const double period = 10.0; // 正弦波周期（秒）
    const int resolution = 100; // 每周期采样点数
    
    std::cout << "开始模拟正弦波CPU占用率..." << std::endl;
    std::cout << "周期: " << period << "秒" << std::endl;
    std::cout << "按Ctrl+C退出程序" << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    
    while (running) {
        // 计算当前时间在周期中的位置
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double phase = 2 * PI * (elapsed / period);
        
        // 计算正弦值并映射到0-100% CPU使用率
        double sine_value = (std::sin(phase) + 1.0) / 2.0; // 0到1之间
        double cpu_usage = sine_value * 100.0;
        
        // 计算当前周期的工作和睡眠时间
        int work_time = static_cast<int>(10 * sine_value); // 10ms最大工作时间
        int sleep_time = 10 - work_time; // 总共10ms为一个时间片
        
        // 模拟CPU工作
        auto work_start = std::chrono::steady_clock::now();
        while (std::chrono::duration<double>(std::chrono::steady_clock::now() - work_start).count() < work_time / 1000.0) {
            // 空循环消耗CPU
            // cpu_intensive_task(10000000); // 添加调用栈
        }
        
        // 睡眠以降低CPU使用率
        if (sleep_time > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
        
        // 每隔一段时间打印当前CPU使用率
        static int counter = 0;
        if (++counter % resolution == 0) {
            std::cout << "当前CPU使用率: " << cpu_usage << "%" << std::endl;
        }
    }
    std::cout << "程序退出" << std::endl;
}

int main() {
    // 注册信号处理函数
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    worker();
    return 0;
}