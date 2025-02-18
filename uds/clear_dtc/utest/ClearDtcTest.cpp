/**
 * @file ClearDtcTest.cpp
 * @author Mujdei Ruben
 * @brief 
 * @version 0.1
 * @date 2024-07-24
 * 
 * @copyright Copyright (c) 2024
 * 
 * PS: Test work with the next data in the file './dtcs.txt':
 *          P0A9B-17 24
 *          P0805-11 2F
 *          C0805-11 2F
 *          B1234-00 00
 *          P0B12-01 10
 *          C0B12-01 10
 * The file is created automatically!!
 */
#include <cstring>
#include <string>
#include <thread>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <gtest/gtest.h>
#include <net/if.h>

#include "../include/ClearDtc.h"
#include "../../../utils/include/CaptureFrame.h"
#include "../../../utils/include/ReceiveFrames.h"
#include "../../../utils/include/NegativeResponse.h"
#include "../../../utils/include/TestUtils.h"
#include "../../../uds/authentication/include/SecurityAccess.h"
#include "Globals.h"
#include "FileGuard.h"

int socket1;
int socket2;
const int id = 0x10FA;

std::vector<uint8_t> seed;

struct ClearDtcTest : testing::Test
{
    ClearDtc* c;
    SecurityAccess* r;
    CaptureFrame* c1;
    Logger* logger;
    std::string strDtcFilePath ="";
    ClearDtcTest()
    {
        v_loadProjectPath();
        logger = new Logger();
        strDtcFilePath = std::string(PROJECT_PATH) + "/backend/ecu_simulation/EngineModule/dtcs.txt";
        c = new ClearDtc(strDtcFilePath ,*logger, socket2);
        r = new SecurityAccess(socket2, *logger);
        c1 = new CaptureFrame(socket1);
    }
    ~ClearDtcTest()
    {
        delete c;
        delete r;
        delete c1;
        delete logger;
    }
};

/* Test for Incorrect message length */
TEST_F(ClearDtcTest, IncorrectMessageLength) {
    std::cerr << "Running IncorrectMessageLength" << std::endl;

    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x14, NegativeResponse::IMLOIF});
    c->clearDtc(0xFA10, {0x02, 0x14, 0x10});
    c1->capture();
    testFrames(result_frame, *c1);

    std::cerr << "Finished IncorrectMessageLength" << std::endl;
}

/* Test for Invalid Group */
TEST_F(ClearDtcTest, InvalidGroup) {
    std::cerr << "Running InvalidGroup" << std::endl;

    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x14, NegativeResponse::ROOR});
    c->clearDtc(0xFA10, {0x04, 0x14, 0x01, 0x0D, 0xDD});
    c1->capture();
    testFrames(result_frame, *c1);

    std::cerr << "Finished InvalidGroup" << std::endl;
}

/* Test for Clear Body DTCs */
TEST_F(ClearDtcTest, ClearBodyDTCs) {
    std::cerr << "Running ClearBodyDTCs" << std::endl;
    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"P0190", {0x24}},
        {"P0115", {0x24}}
    };
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strDtcFilePath);
    v_CreateDummyDtcFile(strDtcFilePath, mapStrVecU16_DtcValues);
    struct can_frame resultFrame = createFrame(0x10FA, {0x02, 0x54, 0x00});
    c->clearDtc(0xFA10, {0x04, 0x14, 0x02, 0x0A, 0xAA});
    c1->capture();
    testFrames(resultFrame, *c1);

    std::cerr << "Finished ClearBodyDTCs" << std::endl;
}

/* Test for Clear Powertrain DTCs */
TEST_F(ClearDtcTest, ClearPowertrainDTCs) {
    std::cerr << "Running ClearPowertrainDTCs" << std::endl;
    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"P0190", {0x24}},
        {"P0115", {0x24}}
    };
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strDtcFilePath);
    v_CreateDummyDtcFile(strDtcFilePath, mapStrVecU16_DtcValues);
    struct can_frame resultFrame = createFrame(0x10FA, {0x02, 0x54, 0x00});
    c->clearDtc(0xFA10, {0x04, 0x14, 0x01, 0x0A, 0xAA});
    c1->capture();
    testFrames(resultFrame, *c1);

    std::cerr << "Finished ClearPowertrainDTCs" << std::endl;
}

/* Test for Clear all DTCs */
TEST_F(ClearDtcTest, ClearAllDTCs) {
    std::cerr << "Running ClearAllDTCs" << std::endl;

    //Ensure dtcs.txt is empty before the test
    std::ofstream ofOutFile(c->str_GetPathToDTC(), std::ios::out | std::ios::trunc);
    ofOutFile.close();

    //Write 3 DTCs into dtcs.txt
    ofOutFile.open(c->str_GetPathToDTC(), std::ios::out | std::ios::trunc);
    ofOutFile << "P0001\nP0002\nP0003\n";  
    ofOutFile.close();

    //Expected CAN response 3 DTCs cleared (0x03)
    struct can_frame stResultFrame = createFrame(0x10FA, {0x02, 0x54, 0x03});
    c->clearDtc(0xFA10, {0x04, 0x14, 0xFF, 0xFF, 0xFF});
    c1->capture();
    testFrames(stResultFrame, *c1);

    std::cerr << "Finished ClearAllDTCs" << std::endl;
}

/* Test for Wrong Path */
TEST_F(ClearDtcTest, WrongPath) {
    std::cerr << "Running WrongPath" << std::endl;

    std::string strDtcFilePathWrong = std::string(PROJECT_PATH) + "/backend/ecu_simulation/WrongPath/dtcs.txt";
    c = new ClearDtc(strDtcFilePathWrong ,*logger, socket2);

    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x14, 0x22});
    c->clearDtc(0xFA10, {0x04, 0x14, 0x01, 0x0A, 0xAA});
    c1->capture();
    testFrames(result_frame, *c1);

    std::cerr << "Finished WrongPath" << std::endl;
}


int main(int argc, char* argv[])
{
    socket1 = createSocket(1);
    socket2 = createSocket(1);
    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    if (socket1 > 0)
    {
        close(socket1);
    }
    if (socket2 > 0)
    {
        close(socket2);
    }
    return result;
}