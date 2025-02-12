#include <cstdint>
#include <fstream>
#include <linux/can.h>
#include <memory>

#include "gtest/gtest.h"
#include <unordered_map>
#include <vector>

#include "../include/RequestDownload.h"

#include "../../../ecu_simulation/BatteryModule/include/BatteryModule.h"
#include "../../../utils/include/CaptureFrame.h"
#include "../../../utils/include/FileManager.h"
#include "../../../utils/include/Logger.h"
#include "../../../mcu/include/MCUModule.h"
#include "../../../utils/include/MemoryManager.h"
#include "../../../utils/include/NegativeResponse.h"
#include "../../../utils/include/ReceiveFrames.h"
#include "../../request_update_status/include/RequestUpdateStatus.h"
#include "../../../uds/authentication/include/SecurityAccess.h"
#include "../../../utils/include/TestUtils.h"

const canid_t kMcuFrameId = 2147487994;
const canid_t kBatteryFrameId = 2147488250;
const int kVCanInterface = 0x00;

const canid_t kMcuReceiverId { 0xFA10 };
const canid_t kBatteryEcuReceiverId { 0xFA11 };

/* 
 Vector with data that will lead to a successful request download request. To be used as template for creating
 data for failure cases
 ToDo: change byte 7 (software version) if after reworking isLatestSoftwareVersion, Ready and WaitDownloadComplete fail
 */
const std::vector<uint8_t> kVecU8_storedData {0x07,0x02,0x01,0x31,0x08,0x00,0x07,0x08};

struct RequestDownloadTest : testing::Test{

    RequestDownloadTest(){
        // Create mcu_data.txt and battery_data.txt
        v_loadProjectPath();
        std::string mcuFilePath {std::string(PROJECT_PATH) + "/backend/mcu/mcu_data.txt"};
        std::string batteryFilePath {std::string(PROJECT_PATH) + "/backend/ecu_simulation/BatteryModule/battery_data.txt"};
        std::ofstream mcuFile {mcuFilePath};
        std::ofstream batteryFile {batteryFilePath};

        // Write to file a map containing the OTA_UPDATE_STATUS_DID
        std::unordered_map<uint16_t, std::vector<uint8_t>> data_map {{OTA_UPDATE_STATUS_DID,{IDLE}}};
        FileManager::writeMapToFile(mcuFilePath, data_map);
        FileManager::writeMapToFile(batteryFilePath, data_map);

        socket = createSocket(kVCanInterface);
        socket2 = createSocket(kVCanInterface);
        spCapturedFrame = std::make_unique<CaptureFrame>(socket2);
        requestDownloadService = std::make_unique<RequestDownloadService>(socket, logger);
        spSecurityAccess = std::make_shared<SecurityAccess>(socket, logger);
    }

    std::shared_ptr<CaptureFrame> spCapturedFrame;
    std::shared_ptr<SecurityAccess> spSecurityAccess;
    std::unique_ptr<RequestDownloadService> requestDownloadService;
    int socket;
    int socket2;
    Logger logger;
};

TEST_F(RequestDownloadTest, MCUAuthenticationFailed){
    struct can_frame expectedFrame = createFrame(kMcuFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::SAD});
    // Receiver must be MCU, use the McuReceiverId defined above (first 2 digits are irrelevant, last 2 must be 10)
    // Stored data is irrelevant at the moment, use an empty vector
    requestDownloadService->requestDownloadRequest(kMcuReceiverId, {});
    spCapturedFrame->capture();
    testFrames(expectedFrame,*spCapturedFrame);

    // Check that stopTimingFlag was called
    EXPECT_EQ(MCU::mcu->stop_flags.find(RequestDownloadService::RDS_SID),MCU::mcu->stop_flags.end());
    EXPECT_EQ(MCU::mcu->active_timers.find(RequestDownloadService::RDS_SID), MCU::mcu->active_timers.end());
}

TEST_F(RequestDownloadTest, ECUAuthenticationFailed){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::SAD});
    // Receiver must be any Ecu, use the EcuReceiverId defined above (last 2 hexes must be 11-14, the first 2 are irrelevant)
    // Stored data is irrelevant at the moment, use an empty vector
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, {});
    spCapturedFrame->capture();
    testFrames(expectedFrame,*spCapturedFrame);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, InsufficientStoredData){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::IMLOIF});
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true so that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);
    // use less than 7 elements in the stored data (empty vector for convenience)
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, {});
    spCapturedFrame->capture();
    testFrames(expectedFrame,*spCapturedFrame);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, IncorrectOtaState){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::CNC});
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a wrong OTA state (not INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT}, kBatteryEcuReceiverId, logger);

    // Use at least 7 elements in the stored data (see the storedData vector above)
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_storedData);
    spCapturedFrame->capture();
    testFrames(expectedFrame,*spCapturedFrame);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, InvalidCompressionOrEncryption){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::ROOR});
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a right OTA state (INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {IDLE}, kBatteryEcuReceiverId, logger);

    /* 
     Use at least 7 elements in the stored data
     This vector does not have 0x00, 0x01, 0x10, 0x11 at idx 2 unlike the constant defined above
     */
    std::vector<uint8_t> kVecU8_invalidCompressionOrEncryptionStoredData = kVecU8_storedData;
    kVecU8_invalidCompressionOrEncryptionStoredData[2] = 0x03;
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_invalidCompressionOrEncryptionStoredData);
    spCapturedFrame->capture(); // this is a frame sent by FileManager::setDIDValue
    spCapturedFrame->capture(); // this is the NRC frame
    testFrames(expectedFrame,*spCapturedFrame);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, InsufficientPayloadSize){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::IMLOIF});
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a right OTA state (INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {IDLE}, kBatteryEcuReceiverId, logger);

    /* 
     Use at least 7 elements in the stored data
     This vector does not have on position 3 a number such that
     the sum of its nibbles + 1 is larger than the stored_data.size
     0xFF for convenience
     */
    std::vector<uint8_t> kVecU8_insufficientPayloadStoredData = kVecU8_storedData;
    kVecU8_insufficientPayloadStoredData[3] = 0xFF;
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_insufficientPayloadStoredData);
    spCapturedFrame->capture(); // this is a frame sent by FileManager::setDIDValue
    spCapturedFrame->capture(); // this is the NRC frame
    testFrames(expectedFrame,*spCapturedFrame);
}

TEST_F(RequestDownloadTest, InvalidMemoryAddress){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::ROOR});
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a right OTA state (INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {IDLE}, kBatteryEcuReceiverId, logger);

    /* 
     Use at least 7 elements in the stored data
     This vector has a memory address (bytes on position 4 and 5) outside the range 
     [DEV_LOOP_PARTITION_1_ADDRESS_START, DEV_LOOP_PARTITION_2_ADDRESS_END], 0 for convenience
     */
    std::vector<uint8_t> kVecU8_invalidMemoryAddressStoredData = kVecU8_storedData;
    kVecU8_invalidMemoryAddressStoredData[4] = 0x00;
    kVecU8_invalidMemoryAddressStoredData[5] = 0x00;
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_invalidMemoryAddressStoredData);
    spCapturedFrame->capture(); // this is a frame sent by FileManager::setDIDValue
    spCapturedFrame->capture(); // this is the NRC frame
    testFrames(expectedFrame,*spCapturedFrame);

    // Ensure that the memory address was the cause of failure
    const int kAddress = RequestDownloadService::getRdsData().address;
    EXPECT_TRUE(kAddress < DEV_LOOP_PARTITION_1_ADDRESS_START || (static_cast<unsigned int>(kAddress) > DEV_LOOP_PARTITION_2_ADDRESS_END));

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, InvalidMemorySize){
    struct can_frame expectedFrame = createFrame(kBatteryFrameId, {0x03, 0x7F, RequestDownloadService::RDS_SID, NegativeResponse::ROOR});
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a right OTA state (INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {IDLE}, kBatteryEcuReceiverId, logger);

    /* 
     Use at least 7 elements in the stored data
     This vector is the only one I found such that this test passes
     */
    std::vector<uint8_t> kVecU8_invalidMemorySizeStoredData = {0,0,0,0x30,0x08,0x00,0,0};
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_invalidMemorySizeStoredData);
    spCapturedFrame->capture(); // this is a frame sent by FileManager::setDIDValue
    spCapturedFrame->capture(); // this is the NRC frame
    testFrames(expectedFrame,*spCapturedFrame);

    // Ensure that the memory size was the cause of failure
    const int kSize = RequestDownloadService::getRdsData().size;
    EXPECT_LT(kSize, 1);


    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

/* TODO: enable after reworking isLatestSoftwareVersion*/
TEST_F(RequestDownloadTest, DISABLED_NotLatestSoftwareVersion){
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a right OTA state (INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {IDLE}, kBatteryEcuReceiverId, logger);

    /* 
     Use at least 7 elements in the stored data
     This vector has a software version (byte 7) less than latest version, using 0 for convenience,
     to be changed if it is not ok
     */
    std::vector<uint8_t> kVecU8_notLatestSoftwareVersionStoredData = kVecU8_storedData;
    kVecU8_notLatestSoftwareVersionStoredData[7] = 0x00;
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_notLatestSoftwareVersionStoredData);
    OtaUpdateStatesEnum postRequestState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, kBatteryEcuReceiverId, logger)[0]);
    EXPECT_EQ(postRequestState, WAIT_DOWNLOAD_FAILED);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, WaitDownloadComplete){
    /*
     Receiver can be either MCU or ECU. Using an ECU because it is more convenient to use ReceiveFrames::setEcuState
     than SecurityAccess::securityAccess
     */

    // Set ecu state to true that ECUAuthentication passes
    ReceiveFrames::setEcuState(true);

    // Simulate a right OTA state (INIT or IDLE) by setting DID value using file manager
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {IDLE}, kBatteryEcuReceiverId, logger);

    // Use at least 7 elements in the stored data (see the storedData vector above)
    requestDownloadService->requestDownloadRequest(kBatteryEcuReceiverId, kVecU8_storedData);
    OtaUpdateStatesEnum postRequestState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, kBatteryEcuReceiverId, logger)[0]);
    EXPECT_EQ(postRequestState, WAIT_DOWNLOAD_COMPLETED);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

TEST_F(RequestDownloadTest, Ready){
    // Receiver must be MCU to achieve READY state post request. Must request security access first
    v_requestSecurityAccess(spSecurityAccess, spCapturedFrame, RequestDownloadService::RDS_SID);

    // OTA state must be INIT to achieve READY state post request
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {INIT}, kMcuReceiverId, logger);

    // Use at least 7 elements in the stored data (see the storedData vector above)
    requestDownloadService->requestDownloadRequest(kMcuReceiverId, kVecU8_storedData);
    OtaUpdateStatesEnum postRequestState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, kMcuReceiverId, logger)[0]);
    EXPECT_EQ(postRequestState, READY);

    // Check that stopTimingFlag was called
    EXPECT_EQ(battery->_ecu->stop_flags.find(RequestDownloadService::RDS_SID),battery->_ecu->stop_flags.end());
    EXPECT_EQ(battery->_ecu->active_timers.find(RequestDownloadService::RDS_SID), battery->_ecu->active_timers.end());
}

int main(int argc, char **argv)
{
testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}