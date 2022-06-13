#include "logger.hpp"
#include <algorithm>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#pragma once
#define RESET       "\033[0m"
#define BLACK       "\033[30m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define BOLDBLACK   "\033[1m\033[30m"
#define BOLDRED     "\033[1m\033[31m"
#define BOLDGREEN   "\033[1m\033[32m"
#define BOLDYELLOW  "\033[1m\033[33m"
#define BOLDBLUE    "\033[1m\033[34m"
#define BOLDMAGENTA "\033[1m\033[35m"
#define BOLDCYAN    "\033[1m\033[36m"
#define BOLDWHITE   "\033[1m\033[37m"

#define INFO(log)                                                  \
    {                                                              \
        std::stringstream ss;                                      \
        ss << log;                                                 \
        bcblog::mtx.lock();                                        \
        std::cout << BOLDBLUE << "[" << bcblog::get_time() << "] " \
                  << "[INFO] " << RESET << ss.str() << std::endl;  \
        bcblog::mtx.unlock();                                      \
        bcblog::logger.info(ss.str());                             \
    }
#define SUCCESS(log)                                                 \
    {                                                                \
        std::stringstream ss;                                        \
        ss << log;                                                   \
        bcblog::mtx.lock();                                          \
        std::cout << BOLDGREEN << "[" << bcblog::get_time() << "] "  \
                  << "[SUCCESS] " << RESET << ss.str() << std::endl; \
        bcblog::mtx.unlock();                                        \
        bcblog::logger.info(ss.str());                               \
    }
#define ERR(log)                                                  \
    {                                                             \
        std::stringstream ss;                                     \
        ss << log;                                                \
        bcblog::mtx.lock();                                       \
        std::cerr << BOLDRED << "[" << bcblog::get_time() << "] " \
                  << "[ERR] " << RESET << ss.str() << std::endl;  \
        bcblog::mtx.unlock();                                     \
        bcblog::logger.error(ss.str());                           \
    }
#define WARN(log)                                                    \
    {                                                                \
        std::stringstream ss;                                        \
        ss << log;                                                   \
        bcblog::mtx.lock();                                          \
        std::cerr << BOLDYELLOW << "[" << bcblog::get_time() << "] " \
                  << "[WARN] " << RESET << ss.str() << std::endl;    \
        bcblog::mtx.unlock();                                        \
        bcblog::logger.warn(ss.str());                               \
    }
#define BRUH(log)                                                     \
    {                                                                 \
        std::stringstream ss;                                         \
        ss << log;                                                    \
        bcblog::mtx.lock();                                           \
        std::cout << BOLDMAGENTA << "[" << bcblog::get_time() << "] " \
                  << "[BRUH] " << RESET << ss.str() << std::endl;     \
        bcblog::mtx.unlock();                                         \
        bcblog::logger.info(ss.str());                                \
    }

namespace bcblog {
    std::mutex mtx;              // NOLINT
    Logger logger("server.log"); // NOLINT

    std::string get_time() { // NOLINT
        time_t t = time(0);
        struct tm* now = localtime(&t);
        char time_cstr[25] = {0};
        strftime(time_cstr, 25, "%a %b %d %H:%M:%S %Y", now);
        return time_cstr;
    }
} // namespace bcblog
