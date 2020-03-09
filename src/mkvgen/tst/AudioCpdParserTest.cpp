#include "MkvgenTestFixture.h"

class AudioCpdParserTest : public MkvgenTestBase {
};

TEST_F(AudioCpdParserTest, audioCpdParser_InvalidInput)
{
    BYTE cpd[] = {0x00, 0x00};
    UINT32 cpdSize = 2;
    DOUBLE samplingFrequency;
    UINT16 channelConfig;

    EXPECT_NE(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(NULL, cpdSize, &samplingFrequency, &channelConfig));
    EXPECT_NE(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(cpd, 0, &samplingFrequency, &channelConfig));
    EXPECT_NE(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(cpd, KVS_AAC_CPD_SIZE_BYTE - 1, &samplingFrequency, &channelConfig));
    EXPECT_NE(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, NULL, &channelConfig));
    EXPECT_NE(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, &samplingFrequency, NULL));
    EXPECT_NE(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, NULL, NULL));
}

TEST_F(AudioCpdParserTest, audioCpdParser_AacCpd)
{
    BYTE cpd[] = {0x12, 0x10};
    UINT32 cpdSize = SIZEOF(cpd);
    DOUBLE samplingFrequency;
    UINT16 channelConfig;

    EXPECT_EQ(STATUS_SUCCESS, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, &samplingFrequency, &channelConfig));
    EXPECT_EQ((DOUBLE) 44100, samplingFrequency);
    EXPECT_EQ(2, channelConfig);
}

TEST_F(AudioCpdParserTest, audioCpdParser_InvalidCpd)
{
    BYTE cpd[] = {0x12, 0x10};
    UINT32 cpdSize = SIZEOF(cpd);
    DOUBLE samplingFrequency;
    UINT16 channelConfig;

    EXPECT_EQ(STATUS_MKV_INVALID_AAC_CPD, getSamplingFreqAndChannelFromAacCpd(NULL, 2, &samplingFrequency, &channelConfig));
    EXPECT_EQ(STATUS_MKV_INVALID_AAC_CPD, getSamplingFreqAndChannelFromAacCpd(NULL, 0, &samplingFrequency, &channelConfig));
    EXPECT_EQ(STATUS_MKV_INVALID_AAC_CPD, getSamplingFreqAndChannelFromAacCpd(cpd, 0, &samplingFrequency, &channelConfig));
    EXPECT_EQ(STATUS_NULL_ARG, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, &samplingFrequency, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, NULL, &channelConfig));
    EXPECT_EQ(STATUS_NULL_ARG, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, NULL, NULL));
}

TEST_F(AudioCpdParserTest, audioCpdParser_invalidSamplingFrequencyIndex)
{
    BYTE cpd[] = {0x07, 0x90}; // sampling frequency index is 15
    UINT32 cpdSize = SIZEOF(cpd);
    DOUBLE samplingFrequency;
    UINT16 channelConfig;

    EXPECT_EQ(STATUS_MKV_INVALID_AAC_CPD_SAMPLING_FREQUENCY_INDEX, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, &samplingFrequency, &channelConfig));
}

TEST_F(AudioCpdParserTest, audioCpdParser_invalidChannelConfig)
{
    BYTE cpd[] = {0x07, 0xc8}; // channel config is 9
    UINT32 cpdSize = SIZEOF(cpd);
    DOUBLE samplingFrequency;
    UINT16 channelConfig;

    EXPECT_EQ(STATUS_MKV_INVALID_AAC_CPD_CHANNEL_CONFIG, getSamplingFreqAndChannelFromAacCpd(cpd, cpdSize, &samplingFrequency, &channelConfig));
}

TEST_F(AudioCpdParserTest, generateAacCpdTest)
{
    BYTE cpd[] = {0x11, 0x90};
    UINT32 samplingFrequency = 48000;
    UINT16 channelConfig = 2;
    BYTE buffer[2];
    UINT32 cpdSize = SIZEOF(buffer);

    EXPECT_EQ(STATUS_NULL_ARG, mkvgenGenerateAacCpd(AAC_LC, samplingFrequency, channelConfig, NULL, cpdSize));
    // invalid cpd size
    EXPECT_EQ(STATUS_INVALID_ARG, mkvgenGenerateAacCpd(AAC_LC, samplingFrequency, channelConfig, buffer, KVS_AAC_CPD_SIZE_BYTE - 1));
    // invalid sampling frequency
    EXPECT_EQ(STATUS_INVALID_ARG, mkvgenGenerateAacCpd(AAC_LC, 12345, channelConfig, buffer, cpdSize));
    // invalid channel config
    EXPECT_EQ(STATUS_INVALID_ARG, mkvgenGenerateAacCpd(AAC_LC, samplingFrequency, 0, buffer, cpdSize));
    EXPECT_EQ(STATUS_INVALID_ARG, mkvgenGenerateAacCpd(AAC_LC, samplingFrequency, MKV_AAC_CHANNEL_CONFIG_MAX, buffer, cpdSize));

    EXPECT_EQ(STATUS_SUCCESS, mkvgenGenerateAacCpd(AAC_LC, samplingFrequency, channelConfig, buffer, cpdSize));
    EXPECT_EQ(0, MEMCMP(cpd, buffer, cpdSize));

    // min sampling frequency index
    EXPECT_EQ(STATUS_SUCCESS, mkvgenGenerateAacCpd(AAC_LC, 96000, channelConfig, buffer, cpdSize));
    // max sampling frequency index
    EXPECT_EQ(STATUS_SUCCESS, mkvgenGenerateAacCpd(AAC_LC, 7350, channelConfig, buffer, cpdSize));
}

