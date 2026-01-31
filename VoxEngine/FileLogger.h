#pragma once
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <mutex>

namespace fs = std::filesystem;

class FileLogger
{
    std::ofstream file;
    std::mutex mtx;
    fs::path filepath;

    void construct(const fs::path& fpath)
    {
        filepath = fpath;
        try
        {
            // Create parent directories if they don't exist
            if (filepath.has_parent_path())
            {
                fs::create_directories(filepath.parent_path());
            }

            // Open the file
            file.open(filepath, std::ios::app);
            if (!file.is_open())
            {
                std::cerr << "[FileLogger]: Cannot open log file\n";
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "[FileLogger]: " << e.what() << " (code: " << e.code() << ")" << "\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "[FileLogger]: " << e.what() << "\n";
        }
    }
public:
    explicit FileLogger(const fs::path& fpath)
    {
        construct(fpath);
    }

    void add(std::string_view line)
    {
        std::lock_guard<std::mutex> lock(mtx);

        if (!file.is_open())
        {
            std::cerr << "[FileLogger]: Log file is not open: " << filepath << "\n";
            return;
        }

        try
        {
            file << line << std::endl;
            file.flush();  // Ensure it's written immediately
        }
        catch (const std::exception& e)
        {
            std::cerr << "[FileLogger]: Write error: " << e.what() << "\n";
        }
    }

    void logException(const std::exception& e, std::string_view context = "")
    {
        std::string message = "EXCEPTION: ";
        if (!context.empty())
        {
            message += context;
            message += ": ";
        }
        message += e.what();
        add(message);
    }
};