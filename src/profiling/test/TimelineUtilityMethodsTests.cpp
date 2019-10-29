//
// Copyright © 2019 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "SendCounterPacketTests.hpp"

#include <SendTimelinePacket.hpp>
#include <TimelineUtilityMethods.hpp>
#include <LabelsAndEventClasses.hpp>

#include <boost/test/unit_test.hpp>

using namespace armnn;
using namespace armnn::profiling;

namespace
{

inline unsigned int OffsetToNextWord(unsigned int numberOfBytes)
{
    unsigned int uint32_t_size = sizeof(uint32_t);

    unsigned int remainder = numberOfBytes % uint32_t_size;
    if (remainder == 0)
    {
        return numberOfBytes;
    }

    return numberOfBytes + uint32_t_size - remainder;
}

void VerifyTimelineLabelBinaryPacket(ProfilingGuid guid,
                                     const std::string& label,
                                     const unsigned char* readableData,
                                     unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);
    unsigned int label_size    = boost::numeric_cast<unsigned int>(label.size());

    // Check the TimelineLabelBinaryPacket header
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily      = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass       = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType        = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId    = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;
    BOOST_CHECK(entityBinaryPacketFamily   == 1);
    BOOST_CHECK(entityBinaryPacketClass    == 0);
    BOOST_CHECK(entityBinaryPacketType     == 1);
    BOOST_CHECK(entityBinaryPacketStreamId == 0);
    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1   = ReadUint32(readableData, offset);
    uint32_t eventBinaryPacketSequenceNumber = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t eventBinaryPacketDataLength     = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(eventBinaryPacketSequenceNumber == 0);
    BOOST_CHECK(eventBinaryPacketDataLength     == 16 + OffsetToNextWord(label_size + 1));

    // Check the decl id
    offset += uint32_t_size;
    uint32_t eventClassDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(eventClassDeclId == 0);

    // Check the profiling GUID
    offset += uint32_t_size;
    uint64_t readProfilingGuid = ReadUint64(readableData, offset);
    BOOST_CHECK(readProfilingGuid == guid);

    // Check the SWTrace label
    offset += uint64_t_size;
    uint32_t swTraceLabelLength = ReadUint32(readableData, offset);
    BOOST_CHECK(swTraceLabelLength == label_size + 1); // Label length including the null-terminator
    offset += uint32_t_size;
    BOOST_CHECK(std::memcmp(readableData + offset,                  // Offset to the label in the buffer
                            label.data(),                           // The original label
                            swTraceLabelLength - 1) == 0);          // The length of the label
    BOOST_CHECK(readableData[offset + swTraceLabelLength] == '\0'); // The null-terminator

    // SWTrace strings are written in blocks of words, so the offset has to be updated to the next whole word
    offset += OffsetToNextWord(swTraceLabelLength);
}

void VerifyTimelineEventClassBinaryPacket(ProfilingGuid guid,
                                          const unsigned char* readableData,
                                          unsigned int& offset)
{
    BOOST_ASSERT(readableData);

    // Utils
    unsigned int uint32_t_size = sizeof(uint32_t);
    unsigned int uint64_t_size = sizeof(uint64_t);

    // Check the TimelineEventClassBinaryPacket header
    uint32_t entityBinaryPacketHeaderWord0 = ReadUint32(readableData, offset);
    uint32_t entityBinaryPacketFamily      = (entityBinaryPacketHeaderWord0 >> 26) & 0x0000003F;
    uint32_t entityBinaryPacketClass       = (entityBinaryPacketHeaderWord0 >> 19) & 0x0000007F;
    uint32_t entityBinaryPacketType        = (entityBinaryPacketHeaderWord0 >> 16) & 0x00000007;
    uint32_t entityBinaryPacketStreamId    = (entityBinaryPacketHeaderWord0 >>  0) & 0x00000007;
    BOOST_CHECK(entityBinaryPacketFamily   == 1);
    BOOST_CHECK(entityBinaryPacketClass    == 0);
    BOOST_CHECK(entityBinaryPacketType     == 1);
    BOOST_CHECK(entityBinaryPacketStreamId == 0);
    offset += uint32_t_size;
    uint32_t entityBinaryPacketHeaderWord1   = ReadUint32(readableData, offset);
    uint32_t eventBinaryPacketSequenceNumber = (entityBinaryPacketHeaderWord1 >> 24) & 0x00000001;
    uint32_t eventBinaryPacketDataLength     = (entityBinaryPacketHeaderWord1 >>  0) & 0x00FFFFFF;
    BOOST_CHECK(eventBinaryPacketSequenceNumber == 0);
    BOOST_CHECK(eventBinaryPacketDataLength     == 12);

    // Check the decl id
    offset += uint32_t_size;
    uint32_t eventClassDeclId = ReadUint32(readableData, offset);
    BOOST_CHECK(eventClassDeclId == 2);

    // Check the profiling GUID
    offset += uint32_t_size;
    uint64_t readProfilingGuid = ReadUint64(readableData, offset);
    BOOST_CHECK(readProfilingGuid == guid);

    offset += uint64_t_size;
}

} // Anonymous namespace

BOOST_AUTO_TEST_SUITE(TimelineUtilityMethodsTests)

BOOST_AUTO_TEST_CASE(SendWellKnownLabelsAndEventClassesTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    BOOST_CHECK_NO_THROW(timelineUtilityMethods.SendWellKnownLabelsAndEventClasses());

    // Commit all packets at once
    sendTimelinePacket.Commit();

    // Get the readable buffer
    auto readableBuffer = mockBufferManager.GetReadableBuffer();
    BOOST_CHECK(readableBuffer != nullptr);
    unsigned int size = readableBuffer->GetSize();
    BOOST_CHECK(size == 136);
    const unsigned char* readableData = readableBuffer->GetReadableData();
    BOOST_CHECK(readableData != nullptr);

    // Utils
    unsigned int offset = 0;

    // First "well-known" label: NAME
    VerifyTimelineLabelBinaryPacket(LabelsAndEventClasses::NAME_GUID,
                                    LabelsAndEventClasses::NAME_LABEL,
                                    readableData,
                                    offset);

    // Second "well-known" label: TYPE
    VerifyTimelineLabelBinaryPacket(LabelsAndEventClasses::TYPE_GUID,
                                    LabelsAndEventClasses::TYPE_LABEL,
                                    readableData,
                                    offset);

    // Third "well-known" label: INDEX
    VerifyTimelineLabelBinaryPacket(LabelsAndEventClasses::INDEX_GUID,
                                    LabelsAndEventClasses::INDEX_LABEL,
                                    readableData,
                                    offset);

    // First "well-known" event class: START OF LIFE
    VerifyTimelineEventClassBinaryPacket(LabelsAndEventClasses::ARMNN_PROFILING_SOL_EVENT_CLASS,
                                         readableData,
                                         offset);

    // First "well-known" event class: END OF LIFE
    VerifyTimelineEventClassBinaryPacket(LabelsAndEventClasses::ARMNN_PROFILING_EOL_EVENT_CLASS,
                                         readableData,
                                         offset);

    // Mark the buffer as read
    mockBufferManager.MarkRead(readableBuffer);
}

BOOST_AUTO_TEST_CASE(DeclareLabelTest)
{
    MockBufferManager mockBufferManager(1024);
    SendTimelinePacket sendTimelinePacket(mockBufferManager);
    TimelineUtilityMethods timelineUtilityMethods(sendTimelinePacket);

    // Try declaring an invalid (empty) label
    BOOST_CHECK_THROW(timelineUtilityMethods.DeclareLabel(""), InvalidArgumentException);

    // Try declaring an invalid (wrong SWTrace format) label
    BOOST_CHECK_THROW(timelineUtilityMethods.DeclareLabel("inv@lid lab€l"), RuntimeException);

    // Declare a valid label
    const std::string labelName = "valid label";
    ProfilingGuid labelGuid = 0;
    BOOST_CHECK_NO_THROW(labelGuid = timelineUtilityMethods.DeclareLabel(labelName));
    // TODO when the implementation of the profiling GUID generator is done, enable the following test
    //BOOST_CHECK(labelGuid != ProfilingGuid(0));

    // TODO when the implementation of the profiling GUID generator is done, enable the following tests
    // Try adding the same label as before
    //ProfilingGuid newLabelGuid = 0;
    //BOOST_CHECK_NO_THROW(labelGuid = timelineUtilityMethods.DeclareLabel(labelName));
    //BOOST_CHECK(newLabelGuid != ProfilingGuid(0));
    //BOOST_CHECK(newLabelGuid == labelGuid);
}

BOOST_AUTO_TEST_SUITE_END()