#ifndef _LOGGER_HPP
#define _LOGGER_HPP

#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <ostream>
#include <regex>
#include <string>
#include <type_traits>
#include <unordered_map>

#ifndef TIMEBUF_SIZE
    #define TIMEBUF_SIZE 1024
#else
    #define TIMEBUF_SIZE_DEFINED
#endif

enum class LogLevel : unsigned int {
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4
};

LogLevel operator|(LogLevel lhs, LogLevel rhs) { // NOLINT
    return static_cast<LogLevel>(
        static_cast<std::underlying_type<LogLevel>::type>(lhs) |
        static_cast<std::underlying_type<LogLevel>::type>(rhs));
}

LogLevel operator&(LogLevel lhs, LogLevel rhs) { // NOLINT
    return static_cast<LogLevel>(
        static_cast<std::underlying_type<LogLevel>::type>(lhs) &
        static_cast<std::underlying_type<LogLevel>::type>(rhs));
}

LogLevel operator^(LogLevel lhs, LogLevel rhs) { // NOLINT
    return static_cast<LogLevel>(
        static_cast<std::underlying_type<LogLevel>::type>(lhs) ^
        static_cast<std::underlying_type<LogLevel>::type>(rhs));
}

LogLevel operator~(LogLevel rhs) { // NOLINT
    return static_cast<LogLevel>(
        ~static_cast<std::underlying_type<LogLevel>::type>(rhs));
}

LogLevel& operator|=(LogLevel& lhs, LogLevel rhs) { // NOLINT
    lhs = static_cast<LogLevel>(
        static_cast<std::underlying_type<LogLevel>::type>(lhs) |
        static_cast<std::underlying_type<LogLevel>::type>(rhs));

    return lhs;
}

LogLevel& operator&=(LogLevel& lhs, LogLevel rhs) { // NOLINT
    lhs = static_cast<LogLevel>(
        static_cast<std::underlying_type<LogLevel>::type>(lhs) &
        static_cast<std::underlying_type<LogLevel>::type>(rhs));

    return lhs;
}

LogLevel& operator^=(LogLevel& lhs, LogLevel rhs) { // NOLINT
    lhs = static_cast<LogLevel>(
        static_cast<std::underlying_type<LogLevel>::type>(lhs) ^
        static_cast<std::underlying_type<LogLevel>::type>(rhs));

    return lhs;
}

class Logger {
private:
    std::mutex mtx;

    static std::string filter(const std::string& str) {
        const static std::regex ansi_escape_code_re(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
        return std::regex_replace(str, ansi_escape_code_re, "");
    }

    void log(std::string message) {
        message = filter(message);

        time_t rawtime;
        struct tm* timeinfo;

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char timef[TIMEBUF_SIZE] = {0};
        strftime(timef, TIMEBUF_SIZE, "%c", timeinfo);

        std::string original_logs = "";
        std::unique_lock<std::mutex> lock(this->mtx);
        {
            std::ifstream logfile(this->logfile_name);
            if (logfile.is_open()) {
                logfile.seekg(0, std::ios::end);
                size_t size = logfile.tellg();
                logfile.seekg(0);
                original_logs.resize(size);
                logfile.read(&original_logs[0], size);
                logfile.close();
            }
        }

        std::ofstream logfile(this->logfile_name);
        if (logfile.is_open()) {
            logfile << "[" << timef << "] " << message << std::endl;
            logfile << original_logs;
            logfile.flush();
            logfile.close();
        } else {
            throw std::runtime_error(strerror(errno));
        }
    }

public:
    std::string logfile_name;
    LogLevel log_level;

    Logger(const std::string& logfile_name, LogLevel log_level = LogLevel::Error | LogLevel::Warning | LogLevel::Info | LogLevel::Debug) :
        logfile_name(logfile_name),
        log_level(log_level) { }

    inline void error(const std::string& message) {
        if (static_cast<bool>(log_level & LogLevel::Error)) {
            log("Error: " + message);
        }
    }

    inline void warn(const std::string& message) {
        if (static_cast<bool>(log_level & LogLevel::Warning)) {
            log("Warning: " + message);
        }
    }

    inline void info(const std::string& message) {
        if (static_cast<bool>(log_level & LogLevel::Info)) {
            log("Info: " + message);
        }
    }

    inline void debug(const std::string& message) {
        if (static_cast<bool>(log_level & LogLevel::Debug)) {
            log("Debug: " + message);
        }
    }
};

#ifndef TIMEBUF_SIZE_DEFINED
    #undef TIMEBUF_SIZE
#else
    #undef TIMEBUF_SIZE_DEFINED
#endif
#endif