#include <cstring>
#include <string>
#include <thread>
#include <fcntl.h>
#include <fstream>
#include <sys/ioctl.h>
#include <gtest/gtest.h>
#include <net/if.h>

#include "../include/WriteDataByIdentifier.h"
#include "../../../uds/authentication/include/SecurityAccess.h"
#include "../../../utils/include/CaptureFrame.h"
#include "../../../utils/include/ReceiveFrames.h"
#include "../../../utils/include/NegativeResponse.h"
#include "../../../utils/include/TestUtils.h"
#include "Globals.h"
#include "FileManager.h"
#include "FileGuard.h"

int socket1;
int socket2;

std::vector<uint8_t> seed;

struct WriteDataByIdentifierTest : testing::Test
{
    WriteDataByIdentifier* w;
    SecurityAccess* r;
    CaptureFrame* c1;
    Logger* logger;
    WriteDataByIdentifierTest()
    {
        v_loadProjectPath();
        logger = new Logger();
        w = new WriteDataByIdentifier(*logger, socket2);
        r = new SecurityAccess(socket2, *logger);
        c1 = new CaptureFrame(socket1);
    }
    ~WriteDataByIdentifierTest()
    {
        delete w;
        delete r;
        delete c1;
        delete logger;
    }
};

/* Test for Incorrect Message Length */
TEST_F(WriteDataByIdentifierTest, IncorrectMessageLength) {
    std::cerr << "Running TestIncorrectMessageLength" << std::endl;
    
    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x2e, NegativeResponse::IMLOIF});

    w->WriteDataByIdentifierService(0xFA10, {0x01, 0x2e});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished TestIncorrectMessageLength" << std::endl;
}

/* Test for MCU security */
TEST_F(WriteDataByIdentifierTest, MCUSecurity) {
    std::cerr << "Running MCUSecurity" << std::endl;
    
    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x2e, NegativeResponse::SAD});

    w->WriteDataByIdentifierService(0xFA10, {0x04, 0x2e, 0xf1, 0x90, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished MCUSecurity" << std::endl;
}

/* Test for ECUs security */
TEST_F(WriteDataByIdentifierTest, ECUsSecurity) {
    std::cerr << "Running ECUsSecurity" << std::endl;
    
    struct can_frame result_frame = createFrame(0x11FA, {0x03, 0x7F, 0x2e, NegativeResponse::SAD});

    w->WriteDataByIdentifierService(0xFA11, {0x04, 0x2e, 0xf1, 0x90, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished ECUsSecurity" << std::endl;
}

/* Test Request Out Of Range MCU */
TEST_F(WriteDataByIdentifierTest, RequestOutOfRangeMCU) {
    std::cerr << "Running RequestOutOfRangeMCU" << std::endl;

    // Out Of Range MCU OTA_UPDATE_STATUS_DID != 0x1111
    FileManager::v_ReplaceOrAddWritableDID(MCU::mcu->writable_MCU_DID, OTA_UPDATE_STATUS_DID, 0x1112);
    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});

    /* Check the security */
    /* Request seed */
    r->securityAccess(0xFA10, {0x02, 0x27, 0x01});

    c1->capture();
    if (c1->frame.can_dlc >= 4)
    {
        seed.clear();
        /* from 3 to pci_length we have the seed generated in response */
        for (int i = 3; i <= c1->frame.data[0]; i++)
        {
            seed.push_back(c1->frame.data[i]);
        }
    }
    /* Compute key from seed */
    for (auto &elem : seed)
    {
        elem = computeKey(elem);
    }
    std::vector<uint8_t> data_frame = {static_cast<uint8_t>(seed.size() + 2), 0x27, 0x02};
    data_frame.insert(data_frame.end(), seed.begin(), seed.end());
    r->securityAccess(0xFA10, data_frame);
    c1->capture();

    w->WriteDataByIdentifierService(0xFA10, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished RequestOutOfRangeMCU" << std::endl;
}

/* Test Request Out Of Range Battery */
TEST_F(WriteDataByIdentifierTest, RequestOutOfRangeBattery) {
    std::cerr << "Running RequestOutOfRangeBattery" << std::endl;

    // Out Of Range Battery OTA_UPDATE_STATUS_DID != 0x1111
    FileManager::v_ReplaceOrAddWritableDID(battery->writable_Battery_DID, OTA_UPDATE_STATUS_DID, 0x1112);

    ReceiveFrames* receiveFrames = new ReceiveFrames(socket2, 0x11, *logger);
    receiveFrames->setEcuState(true);
    struct can_frame result_frame = createFrame(0x11FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});
    w->WriteDataByIdentifierService(0xFA11, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished RequestOutOfRangeBattery" << std::endl;
    delete receiveFrames;
}

/* Test Request Out Of Range Engine */
TEST_F(WriteDataByIdentifierTest, RequestOutOfRangeEngine) {
    std::cerr << "Running RequestOutOfRangeEngine" << std::endl;

    // Out Of Range Engine OTA_UPDATE_STATUS_DID != 0x1111
    FileManager::v_ReplaceOrAddWritableDID(engine->writable_Engine_DID, OTA_UPDATE_STATUS_DID, 0x1112);

    struct can_frame result_frame = createFrame(0x12FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});

    w->WriteDataByIdentifierService(0xFA12, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished RequestOutOfRangeEngine" << std::endl;
}

/* Test Request Out Of Range Doors */
TEST_F(WriteDataByIdentifierTest, RequestOutOfRangeDoors) {
    std::cerr << "Running RequestOutOfRangeDoors" << std::endl;

    // Out Of Range Doors OTA_UPDATE_STATUS_DID != 0x1111
    FileManager::v_ReplaceOrAddWritableDID(doors->writable_Doors_DID, OTA_UPDATE_STATUS_DID, 0x1112);

    struct can_frame result_frame = createFrame(0x13FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});

    w->WriteDataByIdentifierService(0xFA13, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished RequestOutOfRangeDoors" << std::endl;
}

/* Test Request Out Of Range HVAC */
TEST_F(WriteDataByIdentifierTest, RequestOutOfRangeHVAC) {
    std::cerr << "Running RequestOutOfRangeHVAC" << std::endl;

    // Out Of Range HVAC OTA_UPDATE_STATUS_DID != 0x1111
    FileManager::v_ReplaceOrAddWritableDID(hvac->writable_HVAC_DID, OTA_UPDATE_STATUS_DID, 0x1112);

    struct can_frame result_frame = createFrame(0x14FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});

    w->WriteDataByIdentifierService(0xFA14, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished RequestOutOfRangeHVAC" << std::endl;
}

/* Test Corect DID MCU */
TEST_F(WriteDataByIdentifierTest, CorectDIDMCU) {
    std::cerr << "Running CorectDIDMCU" << std::endl;

    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"F1AD", {0xA3, 0x7C, 0x04, 0x37, 0xC9, 0x84, 0x51, 0x9C, 0x53, 0xCF}},
        {"F1AC", {0xCD, 0x59, 0x36, 0x1E, 0x66, 0xBE, 0xA7, 0x1F, 0x3A, 0x94}},
        {"F1AA", {0xBD, 0x97, 0x19, 0x0B, 0x26, 0x2B, 0x80, 0x62, 0x2A}},
        {"F1A8", {0x1C, 0xB8, 0x28, 0xF3, 0x48, 0x42, 0x08, 0x42, 0x3B}},
        {"F1A5", {0xC2, 0x54, 0x0B, 0x11, 0xA5, 0x78, 0x06, 0x8B, 0xD4}},
        {"F1A4", {0xE0, 0x9E, 0xFA, 0xBE, 0xFA, 0xD3, 0x94, 0x31, 0x95, 0xD4}},
        {"F1A9", {0x8E, 0xF8, 0xB4, 0xF9, 0xB1, 0x5D, 0xFF, 0xD6, 0xE0}},
        {"F187", {0x8D, 0x39, 0x1B, 0x71, 0x39, 0xEC, 0x36, 0x49, 0xC4, 0x97, 0x09, 0x8E}},
        {"F1A0", {0xD4, 0x75, 0x96, 0xD5, 0x5A, 0xA6, 0x7C, 0x01, 0xDA}},
        {"F1A2", {0x10}},
        {"F1AB", {0x4D, 0xCA, 0x75, 0xE0, 0xE9, 0x63, 0x70, 0x72, 0x50}},
        {"E001", {0x00}},
        {"EEEE", {0x00}},
        {"F18C", {0x4D, 0x96, 0x7D, 0x6D, 0x02, 0x31, 0x71, 0x5B, 0x64, 0x0A, 0x48, 0xAF}},
        {"F1A1", {0xFE, 0x58, 0x90, 0x9C, 0x37, 0xE9, 0xFD}},
        {"F17F", {0xFB, 0x10, 0xF6, 0x5A, 0x91, 0xB6, 0x4E, 0xEB, 0x2E, 0x25, 0xA6, 0xE9, 0x11, 0xDA, 0x37}},
        {"F190", {0x80, 0x28, 0xB4, 0x15, 0x1D, 0x9F, 0x59, 0x4D, 0xDF, 0xFE, 0x79, 0x95, 0x81, 0x28, 0x30, 0x04}}
    };

    std::string strPath = std::string(PROJECT_PATH) + "/backend/mcu/mcu_data.txt";
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strPath);
    v_CreateDummyDtcFile(strPath, mapStrVecU16_DtcValues);
    FileManager::v_ReplaceOrAddWritableDID(MCU::mcu->writable_MCU_DID, OTA_UPDATE_STATUS_DID, 0xF190);
    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x6e, 0xf1, 0x90});
    w->WriteDataByIdentifierService(0xFA10, {0x04, 0x2e, 0xf1, 0x90, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished CorectDIDMCU" << std::endl;
}

/* Test Corect DID Battery */
TEST_F(WriteDataByIdentifierTest, CorectDIDBattery) {
    std::cerr << "Running CorectDIDBattery" << std::endl;

    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"F1A2", {0x10}},
        {"EEEE", {0x00}},
        {"01F0", {0x4B}},
        {"01E0", {0x0F}},
        {"E001", {0x00}},
        {"01D0", {0x01}},
        {"01C0", {0x5F}},
        {"01B0", {0x0C}},
        {"01A0", {0x26}}
    };
    std::string strPath = std::string(PROJECT_PATH) + "/backend/ecu_simulation/BatteryModule/battery_data.txt";
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strPath);
    v_CreateDummyDtcFile(strPath, mapStrVecU16_DtcValues);
    FileManager::v_ReplaceOrAddWritableDID(battery->writable_Battery_DID, OTA_UPDATE_STATUS_DID, 0x1A0);
    struct can_frame result_frame = createFrame(0x11FA, {0x03, 0x6e, 0x01, 0xa0});
    w->WriteDataByIdentifierService(0xFA11, {0x04, 0x2e, 0x01, 0xa0, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished CorectDIDBattery" << std::endl;
}

/* Test Corect DID Engine */
TEST_F(WriteDataByIdentifierTest, CorectDIDEngine) {
    std::cerr << "Running CorectDIDEngine" << std::endl;

    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"F1A2", {0x10}},
        {"0130", {0x74}},
        {"EEEE", {0x00}},
        {"012C", {0xF7}},
        {"0124", {0x30}},
        {"E001", {0x00}},
        {"0120", {0xEE}},
        {"011C", {0x57}},
        {"0114", {0x48}},
        {"0110", {0xFD}},
        {"010C", {0xF9}},
        {"0134", {0xA1}},
        {"0100", {0x84}}
    };
    std::string strPath = std::string(PROJECT_PATH) + "/backend/ecu_simulation/EngineModule/engine_data.txt";
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strPath);
    v_CreateDummyDtcFile(strPath, mapStrVecU16_DtcValues);
    FileManager::v_ReplaceOrAddWritableDID(engine->writable_Engine_DID, OTA_UPDATE_STATUS_DID, 0x124);
    struct can_frame result_frame = createFrame(0x12FA, {0x03, 0x6e, 0x01, 0x24});

    w->WriteDataByIdentifierService(0xFA12, {0x04, 0x2e, 0x01, 0x24, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished CorectDIDEngine" << std::endl;
}

/* Test Corect DID Doors */
TEST_F(WriteDataByIdentifierTest, CorectDIDDoors) {
    std::cerr << "Running CorectDIDDoors" << std::endl;

    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"F1A2", {0x10}},
        {"EEEE", {0x00}},
        {"E001", {0x00}},
        {"03E0", {0x01}},
        {"03D0", {0x01}},
        {"03C0", {0x00}},
        {"03B0", {0x00}},
        {"03A0", {0x01}}
    };
    std::string strPath = std::string(PROJECT_PATH) + "/backend/ecu_simulation/DoorsModule/doors_data.txt";
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strPath);
    v_CreateDummyDtcFile(strPath, mapStrVecU16_DtcValues);
    FileManager::v_ReplaceOrAddWritableDID(doors->writable_Doors_DID, OTA_UPDATE_STATUS_DID, 0x3A0);
    struct can_frame result_frame = createFrame(0x13FA, {0x03, 0x6e, 0x03, 0xa0});

    w->WriteDataByIdentifierService(0xFA13, {0x04, 0x2e, 0x03, 0xa0, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished CorectDIDDoors" << std::endl;
}

/* Test Corect DID HVAC */
TEST_F(WriteDataByIdentifierTest, CorectDIDHVAC) {
    std::cerr << "Running CorectDIDHVAC" << std::endl;

    const std::map<std::string, std::vector<uint16_t>> mapStrVecU16_DtcValues = {
        {"F1A2", {0x10}},
        {"E001", {0x00}},
        {"04D0", {0x16}},
        {"EEEE", {0x00}},
        {"04C0", {0x32}},
        {"04A0", {0x1D}},
        {"04B0", {0x18}},
        {"0140", {0x05}}
    };
    std::string strPath = std::string(PROJECT_PATH) + "/backend/ecu_simulation/HVACModule/hvac_data.txt";
    // Instantiate the guard to ensure file deletion on scope exit
    FileGuard fileGuard(strPath);
    v_CreateDummyDtcFile(strPath, mapStrVecU16_DtcValues);
    FileManager::v_ReplaceOrAddWritableDID(hvac->writable_HVAC_DID, OTA_UPDATE_STATUS_DID, 0x134);
    struct can_frame result_frame = createFrame(0x14FA, {0x03, 0x6e, 0x01, 0x34});

    w->WriteDataByIdentifierService(0xFA14, {0x04, 0x2e, 0x01, 0x34, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished CorectDIDHVAC" << std::endl;
}

/* Test Module not supported */
TEST_F(WriteDataByIdentifierTest, ModuleNotSupported) {
    std::cerr << "Running ModuleNotSupported" << std::endl;

    struct can_frame result_frame = createFrame(0x15FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});

    w->WriteDataByIdentifierService(0xFA15, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();
    testFrames(result_frame, *c1);
    std::cerr << "Finished ModuleNotSupported" << std::endl;
}

TEST_F(WriteDataByIdentifierTest, ErrorReadingFromFile)
{
    std::string file_name = std::string(PROJECT_PATH) + "/backend/mcu/mcu_data.txt";
    // Check if the file exists and delete it on scope exit
    {
        FileGuard fileGuard(file_name);
    }
    std::string original_content;
    std::ifstream original_file(file_name);
    if (original_file)
    {
        original_content.assign((std::istreambuf_iterator<char>(original_file)),
                                std::istreambuf_iterator<char>());
        original_file.close();
    }
    std::remove(file_name.c_str());

    struct can_frame result_frame = createFrame(0x10FA, {0x03, 0x7F, 0x2e, NegativeResponse::ROOR});
    w->WriteDataByIdentifierService(0xFA10, {0x04, 0x2e, 0x11, 0x11, 0x11});
    c1->capture();

    std::ofstream new_file(file_name);
    if (new_file)
    {
        if (!original_content.empty())
        {
            new_file << original_content;
        }
        new_file.close();
    }
    else
    {
        FAIL() << "Error recreating the file.";
    }
    std::ifstream recreated_file(file_name);
    EXPECT_TRUE(recreated_file.good());

    if (!original_content.empty())
    {
        std::string recreated_content((std::istreambuf_iterator<char>(recreated_file)),
                                      std::istreambuf_iterator<char>());
        EXPECT_EQ(recreated_content, original_content);
    }
    testFrames(result_frame, *c1);
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