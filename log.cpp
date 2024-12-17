#include <string>
#include <sstream>
#include <mutex>
#include <iostream>
#include "log.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <locale>
#include <codecvt>

using json = nlohmann::json;

using namespace std;

std::string ThreadSafeLogger::language = "en";  // Define static member

ThreadSafeLogger::ThreadSafeLogger() {
    std::locale utf8_locale(std::locale(), new std::codecvt_utf8<wchar_t, 0x10ffff, std::consume_header>());
    std::locale::global(utf8_locale);
    // Check if the logs directory exists, if not create it
    system("mkdir -p logs");
    system("mkdir -p logs/archived_logs");
    system("mkdir -p translated_logs");
    system("mkdir -p translated_logs/archived_logs");
    
    std::ifstream f("translations.json");
    if (f.is_open()) {
        translations = json::parse(f);
        if (translations.contains(language)) {
            current_language_translations = translations[language];
        } else {
            std::cerr << "Warning: Language '" << language << "' not found in translations" << std::endl;
            current_language_translations = translations["en"];  // Fallback to English
        }
    }

    // Check if there is already a file in the logs directory
    createTextFile();
    translatedLogStream.imbue(utf8_locale);
}

ThreadSafeLogger::~ThreadSafeLogger() {
    try {
        if (logStream.is_open()) {
            logStream.flush();
            if (!logStream.good()) {
                std::cerr << "Error flushing logStream: " << std::strerror(errno) << std::endl;
            }
            logStream.close();
            if (!logStream.good()) {
                std::cerr << "Error closing logStream: " << std::strerror(errno) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in logStream cleanup: " << e.what() << std::endl;
    }
    
    if (translatedLogStream.is_open()) {
        try {
            translatedLogStream.flush();
            if (!translatedLogStream.good()) {
                std::cerr << "translatedLogStream flush error - Stream state: "
                        << "fail=" << translatedLogStream.fail() 
                        << " bad=" << translatedLogStream.bad()
                        << " eof=" << translatedLogStream.eof()
                        << " UTF conversion error" << std::endl;
            }
            translatedLogStream.clear();
            translatedLogStream.close();
            if (!translatedLogStream.good()) {
                std::cerr << "translatedLogStream close error - Stream state: "
                        << "fail=" << translatedLogStream.fail() 
                        << " bad=" << translatedLogStream.bad()
                        << " eof=" << translatedLogStream.eof() << std::endl;
            }
        } catch (const std::ios_base::failure& e) {
            std::cerr << "Stream operation failed: " << e.what() 
                    << " code: " << e.code() << std::endl;
        }
    }
}

std::wstring ThreadSafeLogger::convertToWString(const std::string& str) {
    std::wstring_convert<codecvt_utf8<wchar_t>> converter;
    std::wstring message = converter.from_bytes(str);
    return message;
}

std::string ThreadSafeLogger::createTextFile() {
    if (logStream.is_open()) logStream.close();
    if (translatedLogStream.is_open()) translatedLogStream.close();

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_tm = *std::localtime(&now_time_t);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y.%m.%d.%H.%M.%S", &now_tm);
    std::string filename = std::string(timestamp) + ".txt";

    {
        // std::lock_guard<std::mutex> fs_lock(fs_mutex);
        logStream.open("logs/" + filename, std::ios::out | std::ios::app);
        translatedLogStream.open("translated_logs/" + filename, std::ios::out | std::ios::app);
        translatedLogStream.imbue(std::locale(translatedLogStream.getloc(),
            new std::codecvt_utf8<wchar_t, 0x10ffff, std::consume_header>()));

        if (!logStream.is_open() || !translatedLogStream.is_open()) {
            throw std::runtime_error("Could not create log files");
        }
    }

    return filename;
}

std::string ThreadSafeLogger::getFileName() {
    // std::lock_guard<std::mutex> lock(fs_mutex);
    FILE* pipe = popen("ls logs/*.txt 2>/dev/null | head -n 1", "r");
    if (!pipe) return createTextFile();
    
    char buffer[128];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
        if (!result.empty() && result[result.length()-1] == '\n') {
            result.erase(result.length()-1);
        }
        size_t pos = result.rfind('/');
        if (pos != std::string::npos) {
            result = result.substr(pos + 1);
        }
    }
    pclose(pipe);
    return result.empty() ? createTextFile() : result;
}

std::string ThreadSafeLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_tm = *std::localtime(&now_time_t);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&now_tm, "[%Y:%m:%d %H:%M:%S.");
    oss << std::setw(3) << std::setfill('0') << now_ms.count() << "]";
    return oss.str();
}

bool ThreadSafeLogger::checkLogSize(const std::string& filename) {
    // std::lock_guard<std::mutex> lock(fs_mutex);
    std::ifstream file("logs/" + filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    return file.tellg() >= (int)MAX_FILE_SIZE;
}

void ThreadSafeLogger::checkCompressedLogs() {
    // std::lock_guard<std::mutex> lock(fs_mutex);
    std::string command = "ls -t logs/archived_logs/*.zip 2>/dev/null | "
                         "tail -n +" + std::to_string(MAX_ARCHIVED_FILES + 1) + " | "
                         "xargs -r rm";
    system(command.c_str());
}

void ThreadSafeLogger::compressLog(const std::string& filename) {
    // std::lock_guard<std::mutex> lock(fs_mutex);
    struct DirPair {
        const char* dir;
        const char* archive_dir;
    };
    
    const DirPair dirs[] = {
        {"logs", "logs/archived_logs"},
        {"translated_logs", "translated_logs/archived_logs"}
    };

    for (const DirPair& pair : dirs) {
        std::string sep = "/";
        std::string sourceFile = pair.dir + sep + filename;
        std::string destinationFile = pair.archive_dir + sep + filename + ".zip";
        
        std::string checkCommand = "test -f " + sourceFile;
        if (system(checkCommand.c_str()) != 0) continue;

        std::string command = "zip -j " + destinationFile + " " + sourceFile + " && rm " + sourceFile;
        system(command.c_str());
    }
    
    checkCompressedLogs();
    createTextFile();
}