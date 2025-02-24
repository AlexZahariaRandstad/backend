#include <cstdlib>
#include <fstream>
#include <gtest/internal/gtest-port.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <gtest/gtest.h>
#include <linux/can.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "../include/TransferData.h"
#include "../../../utils/include/CaptureFrame.h"
#include "../../../utils/include/FileManager.h"
#include "../../../utils/include/MemoryManager.h"
#include "../../../utils/include/Logger.h"
#include "../../../utils/include/NegativeResponse.h"
#include "../../request_update_status/include/RequestUpdateStatus.h"
#include "../../../utils/include/TestUtils.h"

int socket_;
int socket2_;
const int kCanId = 0x1011;

class TransferDataTest : public ::testing::Test {
protected:
    canid_t frame_id;
    std::vector<uint8_t> frame_data;
    Logger mockLogger;
    TransferData* transfer_data;
    CaptureFrame* captured_frame;
    TransferDataTest()
    {
        v_loadProjectPath();
        transfer_data = new TransferData(socket_, mockLogger);
        captured_frame = new CaptureFrame(socket2_);
    }
    ~TransferDataTest()
    {
        delete captured_frame;
    }
    void SetUp(){
        TransferData::expected_block_sequence_number = 0x01;
    }
};

/* Test for computing the checksum */
TEST_F(TransferDataTest, computeChecksumTest){
    const std::vector<uint8_t> kVecU8_data {0x01, 0x02, 0x03};
    const uint8_t checksum = TransferData::computeChecksum(kVecU8_data.data(), kVecU8_data.size());
    EXPECT_EQ(checksum, 0x00);
}

/* Test for Incorrect Message Length */
TEST_F(TransferDataTest, IncorrectMessageLengthTest) {

    // Frames should have at least 3 bytes
    std::vector<uint8_t> invalid_frame_data = {0x02, 0x36};
    std::vector<uint8_t> expected_frame_data = {0x03, 0x7F, 0x36, 0x13};

    transfer_data->transferData(kCanId, invalid_frame_data);
    captured_frame->capture();
    for (int i = 0; i < captured_frame->frame.can_dlc; ++i) {
        EXPECT_EQ(expected_frame_data[i], captured_frame->frame.data[i]);
    }

    // Capture a garbage frame else it will impact the next tests
    captured_frame->capture();
}

/* Test for IncorrectOTAState */
TEST_F(TransferDataTest, IncorrectOTAState) {

    // Do not set the OTA state to one of the expected states: WAIT_DOWNLOAD_COMPLETED, PROCESSING AND PROCESSING_TRANSFER_COMPLETE
    std::vector<uint8_t> invalid_frame_data = {0x02, 0x36, 0x03, 0x02};
    std::vector<uint8_t> expected_frame_data = {0x03, 0x7F, 0x36, NegativeResponse::CNC};

    transfer_data->transferData(kCanId, invalid_frame_data);
    captured_frame->capture();
    for (int i = 0; i < captured_frame->frame.can_dlc; ++i) {
        EXPECT_EQ(expected_frame_data[i], captured_frame->frame.data[i]);
    }
}

/* Test for Wrong block sequence number */
TEST_F(TransferDataTest, WrongBlockSequenceNumberTest) {

    // Set the OTA_UPDATE_STATUS_DID to WAIT_DOWNLOAD_COMPLETED for coverage purposes
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT_DOWNLOAD_COMPLETED}, kCanId, mockLogger, socket_);
    std::vector<uint8_t> invalid_frame_data = {0x02, 0x36, 0x03, 0x02};
    // The first expected block sequence number (byte on position 2) is 0x01. Set to another value to trigger failure
    std::vector<uint8_t> expected_frame_data = {0x03, 0x7F, 0x36, 0x73};

    transfer_data->transferData(kCanId, invalid_frame_data);
    captured_frame->capture(); // this is the setDidValue to WAIT_DOWNLOAD_COMPLETED frame
    captured_frame->capture(); // this is the setDidValue to PROCESSING frame
    captured_frame->capture(); // this is the NRC frame
    for (int i = 0; i < captured_frame->frame.can_dlc; ++i) {
        EXPECT_EQ(expected_frame_data[i], captured_frame->frame.data[i]);
    }
}

/* Test for PositiveResponse */
TEST_F(TransferDataTest, PositiveResponseTest) {

    // Set the OTA_UPDATE_STATUS_DID to WAIT_DOWNLOAD_COMPLETED for coverage purposes
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT_DOWNLOAD_COMPLETED}, kCanId, mockLogger, socket_);
    std::vector<uint8_t> frame_data = {0x02, 0x36, 0x01, 0x02, 0x33};
    std::vector<uint8_t> expected_frame_data = {0x03, 0x76, 0x01, PROCESSING};

    transfer_data->transferData(kCanId, frame_data);
    captured_frame->capture(); // this is the setDidValue to WAIT_DOWNLOAD_COMPLETED frame
    captured_frame->capture(); // this is the setDidValue to PROCESSING frame
    captured_frame->capture(); // this is the NRC frame
    for (int i = 0; i < captured_frame->frame.can_dlc; ++i) {
        EXPECT_EQ(expected_frame_data[i], captured_frame->frame.data[i]);
    }

    // No checksums were added
    EXPECT_EQ(TransferData::getChecksums().size(), 0);
}

/* 
 * Test for DataTransferComplete. 
 * @warning Make sure that you ran create_sd_card.sh before running this test! 
 */
TEST_F(TransferDataTest, DataTransferComplete) {

    // Set the OTA_UPDATE_STATUS_DID to PROCESSING_TRANSFER_COMPLETE to cover the data transfer complete section
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {PROCESSING_TRANSFER_COMPLETE}, kCanId, mockLogger, socket_);
    std::vector<uint8_t> frame_data = {0x02, 0x36, 0x01, 0x02, 0x33};
    std::vector<uint8_t> expected_frame_data = {0x03, 0x76, 0x01, PROCESSING_TRANSFER_COMPLETE};

    transfer_data->transferData(kCanId, frame_data);
    captured_frame->capture(); // this is the setDidValue to PROCESSING_TRANSFER_COMPLETE frame 
    captured_frame->capture(); // this is the expected response frame
    
    for (int i = 0; i < captured_frame->frame.can_dlc; ++i) {
        EXPECT_EQ(expected_frame_data[i], captured_frame->frame.data[i]);
    }
}

/* Test for DataTransferFail */
TEST_F(TransferDataTest, DataTransferFailed) {

    // Set the OTA_UPDATE_STATUS_DID to PROCESSING_TRANSFER_COMPLETE to cover the data transfer fail section
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {PROCESSING_TRANSFER_COMPLETE}, kCanId, mockLogger, socket_);
    std::vector<uint8_t> expected_frame_data = {0x03, 0x7F, TransferData::TD_SID, NegativeResponse::TDS};
    // Using an extremely large frame so that MemoryManager::writeToAddress fails as availableMemory will fail
    std::vector<uint8_t> frame_data (123456789,0x01);

    transfer_data->transferData(kCanId, frame_data);
    captured_frame->capture(); // this is the setDidValue to PROCESSING_TRANSFER_COMPLETE frame 
    captured_frame->capture(); // this is the expected NRC frame

    const OtaUpdateStatesEnum kOtaState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, kCanId, mockLogger)[0]);
    EXPECT_EQ(kOtaState, PROCESSING_TRANSFER_FAILED);

}

/* Test for process transfer failed */
TEST_F(TransferDataTest, ProcessTransferFailed) {

    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT_DOWNLOAD_COMPLETED}, MCU_ID, mockLogger, socket_);
    std::vector<uint8_t> vecU8_preprocessedData {0x01, 0x02, 0x03};

    // Failure occurs as we did not set an ecu zip file path
    TransferData::processDataForTransfer(kCanId, vecU8_preprocessedData, socket_, mockLogger);
    const OtaUpdateStatesEnum kOtaState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, kCanId, mockLogger)[0]);
    EXPECT_EQ(kOtaState, PROCESSING_TRANSFER_FAILED);
}

/* Test for process transfer complete. */
TEST_F(TransferDataTest, ProcessDataForTransferComplete) {

    // Data to be used for processing
    std::vector<uint8_t> vecU8_preprocessedData1 {0x01};
    std::vector<uint8_t> vecU8_preprocessedData2 {0x02, 0x03};
    std::vector<uint8_t> vecU8_preprocessedData {0x01, 0x02, 0x03};

    // Zip file related constants
    const std::string kStr_zipFilePath { PROJECT_PATH + std::string("/ECU_BATTERY_SW_VERSION_0.zip") };
    const std::string kStr_zipCreateCmd { std::string("touch ") + kStr_zipFilePath };
    const std::string kStr_zipDeleteCmd { std::string("rm ") + kStr_zipFilePath};

    // Set OTA_UPDATE_STATUS_DID to WAIT_DOWNLOAD_COMPLETED, the state before starting to process data
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT_DOWNLOAD_COMPLETED}, MCU_ID, mockLogger, socket_);

    // Create the zip file, write data to it, and get its path
    system(kStr_zipCreateCmd.c_str());
    MemoryManager::writeToFile(vecU8_preprocessedData, kStr_zipFilePath, mockLogger);
    std::string ecuPath;
    FileManager::getEcuPath(0x11, ecuPath, 0, mockLogger, "0");

    // Signal the start of processing data
    testing::internal::CaptureStdout();
    TransferData::processDataForTransfer(kCanId, vecU8_preprocessedData, socket_, mockLogger);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Data processing for transfer started."), std::string::npos);

    // Set OTA_UPDATE_STATUS_DID to PROCESSING
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {PROCESSING}, MCU_ID, mockLogger, socket_);

    // Process data by 2 transfers so that we cover the complete processing code section
    TransferData::processDataForTransfer(kCanId, vecU8_preprocessedData1, socket_, mockLogger);
    TransferData::processDataForTransfer(kCanId, vecU8_preprocessedData2, socket_, mockLogger);

    // The OTA state of MCU should be PROCESSING_TRANSFER_COMPLETE
    const OtaUpdateStatesEnum kOtaState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, MCU_ID, mockLogger)[0]);
    EXPECT_EQ(kOtaState, PROCESSING_TRANSFER_COMPLETE);

    // Delete the zip file
    system(kStr_zipDeleteCmd.c_str());
}

/* Test for process transfer incomplete. */
TEST_F(TransferDataTest, ProcessDataForTransferIncomplete) {

    // Data to be used for processing
    std::vector<uint8_t> vecU8_preprocessedData {0x01, 0x02, 0x03};

    // Zip file related constants
    const std::string kStr_zipFilePath { PROJECT_PATH + std::string("/ECU_BATTERY_SW_VERSION_0.zip") };
    const std::string kStr_zipCreateCmd { std::string("touch ") + kStr_zipFilePath };
    const std::string kStr_zipDeleteCmd { std::string("rm ") + kStr_zipFilePath};

    // Set OTA_UPDATE_STATUS_DID to WAIT_DOWNLOAD_COMPLETED, the state before starting to process data
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT_DOWNLOAD_COMPLETED}, MCU_ID, mockLogger, socket_);

    // Create the zip file, write data to it, and get its path
    system(kStr_zipCreateCmd.c_str());
    MemoryManager::writeToFile(vecU8_preprocessedData, kStr_zipFilePath, mockLogger);
    std::string ecuPath;
    FileManager::getEcuPath(0x11, ecuPath, 0, mockLogger, "0");

    // Signal the start of processing data
    testing::internal::CaptureStdout();
    TransferData::processDataForTransfer(kCanId, vecU8_preprocessedData, socket_, mockLogger);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Data processing for transfer started."), std::string::npos);

    // Set OTA_UPDATE_STATUS_DID to PROCESSING
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {PROCESSING}, MCU_ID, mockLogger, socket_);

    // Process data by 1 transfer so that we cover the incomplete processing code section
    TransferData::processDataForTransfer(kCanId, vecU8_preprocessedData, socket_, mockLogger);

    // The OTA state of MCU should not be PROCESSING_TRANSFER_COMPLETE
    const OtaUpdateStatesEnum kOtaState = static_cast<OtaUpdateStatesEnum>(FileManager::getDidValue(OTA_UPDATE_STATUS_DID, MCU_ID, mockLogger)[0]);
    EXPECT_NE(kOtaState, PROCESSING_TRANSFER_COMPLETE);

    // Delete the zip file
    system(kStr_zipDeleteCmd.c_str());
}

/* Test for cyclicity of expected block sequence number*/
TEST_F(TransferDataTest, ExpectedBlockSequenceNumberCyclicityTest){

    // Set the OTA_UPDATE_STATUS_DID to WAIT_DOWNLOAD_COMPLETED for coverage purposes
    FileManager::setDidValue(OTA_UPDATE_STATUS_DID, {WAIT_DOWNLOAD_COMPLETED}, kCanId, mockLogger, socket_);
    std::vector<uint8_t> frame_data = {0x02, 0x36, 0x01, 0x02, 0x33};

    for (uint8_t i = 0x00; i < 0xFF; i++){
        frame_data[2] = i+1;
        transfer_data->transferData(kCanId, frame_data);
    }

    // After 255 data transfers, the expected block sequence number should be back to 0x01
    EXPECT_EQ(TransferData::expected_block_sequence_number, 0x01);
}

int main(int argc, char **argv) {
    socket_ = createSocket(0);
    socket2_ = createSocket(0);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
