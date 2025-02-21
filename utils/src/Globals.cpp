#include "Globals.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <string>

std::string PROJECT_PATH;

void v_loadProjectPath() {
    std::filesystem::path configIniFilePath = std::filesystem::current_path().remove_filename();
    while (!configIniFilePath.empty() && configIniFilePath.filename().string() != std::string("backend")){
        configIniFilePath = configIniFilePath.parent_path();
    }

    // Sanity check, ensure that backend was found
    if (configIniFilePath.empty()){
        throw std::runtime_error("Root directory reached when loading project path. Check that this function was called from a file inside Poc/src/backend or any of its subdirectories");
    }

    configIniFilePath /= "config/config.ini";
    std::ifstream file(configIniFilePath.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config.ini");
    }

    std::string line, key = "PROJECT_PATH=";
    while (std::getline(file, line)) {
        if (line.rfind(key, 0) == 0) {
            PROJECT_PATH = line.substr(key.size());
            return;
        }
    }

    throw std::runtime_error("PROJECT_PATH not found in config.ini");
}

std::string to_lowercase(const std::string& str) {
	std::string lower_str = str;
	std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), [](unsigned char c){ return std::tolower(c); });
	return lower_str;
}

int countDigits(int number) {
    int digits = 0;
    if (number < 0) 
        return 0;
    while (number) {
        number /= 10;
        digits++;
    }
    return digits;
}

void stopProcess(std::string process_name)
{
    /* Use popen to capture the output of the command */
    FILE* fp = popen(("pgrep -f '" + process_name + "' | wc -l").c_str(), "r");
    if (fp == nullptr) {
        std::cerr << "Failed to run command" << std::endl;
        return;
    }

    int count = 0;
    /* Read the output of the command and check if it is valid */
    if (fscanf(fp, "%d", &count) != 1) {
        std::cerr << "Failed to read process count" << std::endl;
        pclose(fp);
        return;
    }
    pclose(fp);

    /* Loop while there are more than 2 processes running */
    while (count > 2) {
        std::cout << "More than one process found. Killing old instances..." << std::endl;

        /* Kill the oldest process */
        int result = system(("pkill -o -f '" + process_name + "'").c_str());
        if (result == 0) {
            std::cout << "Old process terminated successfully." << std::endl;
        } else {
            std::cerr << "Failed to terminate old process." << std::endl;
        }

        /* Reload the process count after killing one process */
        fp = popen(("pgrep -f '" + process_name + "' | wc -l").c_str(), "r");
        if (fp == nullptr) {
            std::cerr << "Failed to run command" << std::endl;
            return;
        }
        if (fscanf(fp, "%d", &count) != 1) {
            std::cerr << "Failed to read process count" << std::endl;
            pclose(fp);
            return;
        }
        pclose(fp);
    }
}

int createUdpSocket() {
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        std::cerr << "Error creating UDP socket!" << std::endl;
        return -1;
    }

    int broadcastEnable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        std::cerr << "Error setting socket options for broadcast!" << std::endl;
        close(udp_socket);
        return -1;
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error binding UDP socket!" << std::endl;
        close(udp_socket);
        return -1;
    }

    return udp_socket;
}
