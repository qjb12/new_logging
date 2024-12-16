#include <string>
#include <sstream>
#include <mutex>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ThreadSafeLogger {
private:
    std::mutex log_mutex;
    // static std::mutex fs_mutex;  // Add this line for filesystem operations
    json translations;
    std::ofstream logStream;
    std::ofstream translatedLogStream;
    static constexpr size_t MAX_FILE_SIZE = 256;
    static constexpr int MAX_ARCHIVED_FILES = 2;

public:
    template<typename... Args>
    static void staticLog(const char* format, Args... args) {
        static ThreadSafeLogger logger;  // Thread-safe initialization
        logger.log(format, args...);
    }
    
    // Make constructor private to prevent external instantiation
private:
    ThreadSafeLogger();
public:
    ~ThreadSafeLogger();

    // Base template to handle different types of arguments
    template<typename... Args>
    void log(const char* format, Args... args) {
        // Get both locks in a consistent order to prevent deadlock
        // std::lock_guard<std::mutex> fs_lock(fs_mutex);
        log_mutex.lock();
        
        std::string timestamp = getCurrentTimestamp();
        
        if (!logStream.is_open() || !translatedLogStream.is_open()) {
            createTextFile();  // Now safe to call since we hold both locks
        }
        
        std::ostringstream message_stream;
        message_stream << timestamp << " ";
        
        std::string fmt_str = format;
        std::string translated_fmt = fmt_str;
        if (translations.contains(fmt_str)) {
            translated_fmt = translations[fmt_str].get<std::string>();
        }
        
        // Build original message
        std::ostringstream original_stream;
        buildMessage(original_stream, fmt_str.c_str(), args...);
        
        // Build translated message
        std::ostringstream translated_stream;
        buildMessage(translated_stream, translated_fmt.c_str(), args...);
        
        // Write to console and files
        std::cout << timestamp << " " << original_stream.str() << std::endl;
        
        logStream << timestamp << " " << original_stream.str() << std::endl;
        translatedLogStream << timestamp << " " << translated_stream.str() << std::endl;
        
        std::string currentFile = getFileName();
        if (checkLogSize(currentFile)) {
            compressLog(currentFile);
            createTextFile();
        }
        // Unlock the mutex
        log_mutex.unlock();
    }

private:
    std::string createTextFile();
    std::string getFileName();
    std::string getCurrentTimestamp();
    bool checkLogSize(const std::string& filename);
    void checkCompressedLogs();
    void compressLog(const std::string& filename);
    
    // Recursive template method to build the message
    void buildMessage(std::ostringstream& stream, const char* format) {
        stream << format;
    }

    template<typename T, typename... Args>
    void buildMessage(std::ostringstream& stream, const char* format, T first, Args... rest) {
        while (*format) {
            if (*format == '%') {
                // Found a placeholder, insert the variable
                stream << first;
                buildMessage(stream, format + 1, rest...);
                return;
            }
            stream << *format;
            ++format;
        }
    }
};