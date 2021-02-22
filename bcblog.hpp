#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <mutex>
#include <algorithm>

#define RESET   "\033[0m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLDBLACK   "\033[1m\033[30m"
#define BOLDRED     "\033[1m\033[31m"
#define BOLDGREEN   "\033[1m\033[32m"
#define BOLDYELLOW  "\033[1m\033[33m"
#define BOLDBLUE    "\033[1m\033[34m"
#define BOLDMAGENTA "\033[1m\033[35m"
#define BOLDCYAN    "\033[1m\033[36m"
#define BOLDWHITE   "\033[1m\033[37m"

#define INFO(log) bcblog::mtx.lock(); std::cout << BOLDBLUE << "[" << bcblog::get_time() << "] " << "[INFO] " << RESET << log << std::endl; bcblog::mtx.unlock()
#define SUCCESS(log) bcblog::mtx.lock(); std::cout << BOLDGREEN << "[" << bcblog::get_time() << "] " << "[SUCCESS] " << RESET << log << std::endl; bcblog::mtx.unlock()
#define ERR(log) bcblog::mtx.lock(); std::cerr << BOLDRED << "[" << bcblog::get_time() << "] " << "[ERR] " << RESET << log << std::endl; bcblog::mtx.unlock()
#define WARN(log) bcblog::mtx.lock(); std::cout << BOLDYELLOW << "[" << bcblog::get_time() << "] " << "[WARN] " << RESET << log << std::endl; bcblog::mtx.unlock()

namespace bcblog {
    std::mutex mtx;

    std::string get_time() {
        time_t t = time(0);
        char *dt = ctime(&t);
        // struct tm *now = localtime(&t);
        // std::stringstream sstm;
        // sstm << (now->tm_hour) << ':' << (now->tm_min) << ':' << now->tm_sec;
        // std::string s = sstm.str();
        std::string s(dt);
        s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
        return s;
    }
}