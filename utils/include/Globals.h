#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define UDP_IP "192.168.241.255"
#define UDP_PORT 5000

extern std::string PROJECT_PATH;

/**
 * @brief find the config.ini file and load project path from it
 */
void v_loadProjectPath();

/**
 * @brief Convert all the letters to lowercase
 * 
 */
std::string to_lowercase(const std::string& str);

/**
 * @brief Count the number of digits in a number
 * 
 * @param number The integer whose digits are to be counted.
 * @return The number of digits for a given integer.
 */
int countDigits(int number);
/**
 * @brief Method to stop all the processes with the given name, the current one.
 * 
 * @param process_name Process name to be stopped.
 */
void stopProcess(std::string process_name);

int createUdpSocket();

/* Enumeration for frame types */
enum FrameType {
    DATA_FRAME,
    REMOTE_FRAME,
    ERROR_FRAME,
    OVERLOAD_FRAME
};

struct TransferPacket {
    uint32_t id;
    uint16_t pci;
    uint8_t sid;
    uint8_t block_counter;
    std::vector<uint8_t> data;
};

#endif
