#include <iostream>
#include <string>
#include <sstream>
#include <ctime>

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

#define UNUSED(expr) do { (void)(expr); } while (0)
#define INFO(expr) std::cout << BOLDBLUE << get_time() << " [INFO] " << RESET << expr << std::endl
#define SUCCESS(expr) std::cout << BOLDGREEN << get_time() << " [SUCCESS] " << RESET << expr << std::endl
#define ERR(expr) std::cerr << BOLDRED << get_time() << " [ERR] " << RESET << expr << std::endl
#define WARN(expr) std::cout << BOLDYELLOW << get_time() << " [WARN] " << RESET << expr << std::endl

std::string get_time() {
    time_t t = time(0);
    struct tm *now = localtime(&t);
    std::stringstream sstm;
    sstm << (now->tm_hour) << ':' << (now->tm_min) << ':' << now->tm_sec;
    std::string s = sstm.str();
    return s;
}
