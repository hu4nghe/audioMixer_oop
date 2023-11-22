#ifndef AUDIOMIXER_DELTACAST_H
#define AUDIOMIXER_DELTACAST_H

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Dv.h"
#include "VideoMasterHD_Dv_Audio.h"

#include "audioMixer_base.h"

#pragma region Handles
class boardHandle
{
    std::uint32_t  boardID{0};
             void* handle {nullptr};
public:
    boardHandle()
    {
        if (VHD_OpenBoardHandle(boardID, &handle, nullptr, 0))
            throw std::runtime_error("Cannot open DELTA board handle.");
        else
        {
            ULONG boardType{0};
            VHD_GetBoardProperty(handle, VHD_CORE_BP_BOARD_TYPE, &boardType);
            bool isHDMI = ((boardType == VHD_BOARDTYPE_HDMI)            ||
                           (boardType == VHD_BOARDTYPE_HDMI20)          ||
                           (boardType == VHD_BOARDTYPE_FLEX_HMI)        ||
                           (boardType == VHD_BOARDTYPE_MIXEDINTERFACE));
            if (!isHDMI)
                throw std::runtime_error("ERROR : The selected board is not a HDMI or DVI board");
        }
    }
   ~boardHandle(){ VHD_CloseBoardHandle(handle); }

    [[nodiscard]] auto getHandle() const -> decltype(handle)  { return handle; }
};
class streamHandle
{
    void* handle{nullptr};
public: 
    streamHandle(const boardHandle& brdHdl)
    {
        if (VHD_OpenStreamHandle(brdHdl.getHandle(), VHD_ST_RX0, VHD_DV_STPROC_JOINED, nullptr, &handle, nullptr))
            throw std::runtime_error("ERROR : Cannot open RX0 stream on HDMI / DVI board handle.");
        else
            if (VHD_SetStreamProperty(handle, VHD_DV_SP_MODE, VHD_DV_MODE_HDMI))
                throw std::runtime_error("ERROR : Cannot configure RX0 stream primary mode.");
    }
   ~streamHandle()
   { 
       VHD_CloseStreamHandle(handle); 
   }
    [[nodiscard]] auto getHandle() const -> decltype(handle)  { return handle; }
};
#pragma endregion

#pragma region Delta Cast Module
class deltaCast : public audioMixerModule_base
{
    //handles
    std::unique_ptr< boardHandle>   boardHdl;
    std::unique_ptr<streamHandle>   streamHdl;
    //HDMI audio infos
    VHD_DV_AUDIO_INFOFRAME HDMIAudioInfoFrame{};
    VHD_DV_AUDIO_AES_STS   HDMIAudioAESSts   {};

    auto byteCombineToShort(const std::uint8_t* sourceAudio,
                            const std:: size_t  sourceSize) -> std::vector<short>;
public:
	deltaCast(const outputParameter& outputCfg);

	void start() override;
	void stop () override;

            void streamConfig     ();
            void startStream      ();
    std:: size_t getSampleRate    () const;
    std::uint8_t getChannelNumbers() const;
    
};
#pragma endregion

#pragma region IMPL Delta Cast Module

                    deltaCast::deltaCast         (const outputParameter& outputCfg)
	:	module(outputCfg)
{
    ULONG boardNumber {0};
    if (VHD_GetApiInfo(nullptr, &boardNumber))
        throw std::runtime_error("Cannot query VideoMaster information.");
    else
    {
        if(boardNumber)
        {
             boardHdl = std::make_unique< boardHandle>();
            streamHdl = std::make_unique<streamHandle>(*boardHdl);
        }    
        else
            throw std::runtime_error("No Delta board detected.");
    }
}
inline std:: size_t deltaCast::getSampleRate     () const
{
    const int code{ HDMIAudioInfoFrame.SamplingFrequency };
    std::size_t sampleRate{};
    switch (code)
    {
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_REF_STREAM_HEADER:
    {
        //We need to do a reference to stream header.
        const int codeRef{ HDMIAudioAESSts.SamplingFrequency };
        switch (codeRef)
        {
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_44100HZ:
            sampleRate = 44100;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_48000HZ:
            sampleRate = 48000;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_32000HZ:
            sampleRate = 32000;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_88200HZ:
            sampleRate = 88200;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_768000HZ:
            sampleRate = 768000;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_96000HZ:
            sampleRate = 96000;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_176000HZ:
            sampleRate = 176000;
            break;
        case VHD_DV_AUDIO_AES_STS_SAMPLING_FREQ_192000HZ:
            sampleRate = 192000;
            break;
        default:
            throw std::invalid_argument("Invalide sample rate code.");
            break;
        }
        break;
    }
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_32000HZ:
        sampleRate = 32000;
        break;
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_44100HZ:
        sampleRate = 44100;
        break;
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_48000HZ:
        sampleRate = 48000;
        break;
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_88200HZ:
        sampleRate = 88200;
        break;
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_96000HZ:
        sampleRate = 96000;
        break;
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_176400HZ:
        sampleRate = 176400;
        break;
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_192000HZ:
        sampleRate = 192000;
        break;
    default:
        throw std::invalid_argument("Invalide sample rate code.");
        break;
    }
    return sampleRate;


}
inline std::uint8_t deltaCast::getChannelNumbers () const
{
    const std::uint32_t code{ HDMIAudioInfoFrame.ChannelCount };
    std::uint8_t channelNumber{};

    switch (code)
    {
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_REF_STREAM_HEADER:
    {
        channelNumber = HDMIAudioAESSts.ChannelNb;
        break;
    }
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_2:
        channelNumber = 2;
        break;
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_3:
        channelNumber = 3;
        break;
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_4:
        channelNumber = 4;
        break;
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_5:
        channelNumber = 5;
        break;
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_6:
        channelNumber = 6;
        break;
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_7:
        channelNumber = 7;
        break;
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_8:
        channelNumber = 8;
        break;
    default:
        throw std::invalid_argument("Invalide channel number code.");
        break;
    }
    return channelNumber;
}
inline         void deltaCast::start             ()
{
    this->streamConfig();
    this->startStream ();
}
inline         void deltaCast::stop              ()
{
    VHD_StopStream(streamHdl->getHandle());
    active = false;
}
inline         void deltaCast::streamConfig      ()
{
    VHD_DV_MODE DvMode{NB_VHD_DV_MODES};
    do
    {
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_MODE, (ULONG*)&DvMode);
    } while (DvMode != VHD_DV_MODE_HDMI);

    if (DvMode == VHD_DV_MODE_HDMI)
    {
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_CORE_SP_BUFFER_PACKING , VHD_BUFPACK_VIDEO_RGB_32);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_CORE_SP_TRANSFER_SCHEME, VHD_TRANSFER_SLAVED);
        /* Get auto-detected resolution */
        ULONG Height{0}, Width{0}, RefreshRate{0}, PxlClk{0};
        BOOL32 Interlaced{0};
        VHD_DV_CS InputCS{};
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_WIDTH ,         &Width);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_HEIGHT,         &Height);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INTERLACED   , (ULONG*)&Interlaced);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_REFRESH_RATE ,         &RefreshRate);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INPUT_CS     , (ULONG*)&InputCS);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_PIXEL_CLOCK  ,         &PxlClk);
        /* Configure stream.*/
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_WIDTH , Width);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_HEIGHT, Height);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INTERLACED   , Interlaced);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_REFRESH_RATE , RefreshRate);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INPUT_CS     , InputCS);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_PIXEL_CLOCK  , PxlClk);
    }
    else
        throw std::runtime_error("ERROR : Cannot detect if incoming mode is HDMI.");
}
inline         void deltaCast::startStream       ()
{
    
    HANDLE slotHandle   {nullptr};
    BYTE*  pBuffer      {nullptr};
    BYTE*  pAudioBuffer {nullptr};
    ULONG  bufferSize   {0};
    audioQueue<float> queue(outputConfig);
    audio->push_back(queue);


    /* Start stream */
    VHD_StartStream(streamHdl->getHandle());
    std::print("HDMI audio reception started\n");
    active = true;
    bool set{ false };
    /* Reception loop */
    while (true)
    {
        /* Try to lock next slot */
        auto Result = VHD_LockSlotHandle(streamHdl->getHandle(), &slotHandle);
        if (Result == VHDERR_NOERROR)
        {
            VHD_DV_AUDIO_TYPE HDMIAudioType{};

            VHD_GetSlotBuffer     (slotHandle, VHD_DV_BT_VIDEO, &pBuffer, &bufferSize);
            VHD_GetSlotDvAudioInfo(slotHandle, &HDMIAudioType, &HDMIAudioInfoFrame, &HDMIAudioAESSts);

            if (HDMIAudioType != VHD_DV_AUDIO_TYPE_NONE)
            {
                if (HDMIAudioAESSts.LinearPCM == VHD_DV_AUDIO_AES_SAMPLE_STS_LINEAR_PCM_SAMPLE)
                {
                    VHD_SlotExtractDvPCMAudio(slotHandle, VHD_DVAUDIOFORMAT_16, 0x3, nullptr     , &bufferSize);//get buffer size
                    pAudioBuffer = new UBYTE[bufferSize];
                    VHD_SlotExtractDvPCMAudio(slotHandle, VHD_DVAUDIOFORMAT_16, 0x3, pAudioBuffer, &bufferSize);

                    auto temp = byteCombineToShort(pAudioBuffer, bufferSize);
                    std::vector<float> floatData(temp.size());
                    src_short_to_float_array(temp.data(), floatData.data(), temp.size());

                    (*audio)[0].push(floatData, getSampleRate(), getChannelNumbers());
                }
                if (pAudioBuffer)
                {
                    delete[] pAudioBuffer;
                    pAudioBuffer = nullptr;
                }
            }
            /* Unlock slot */
            VHD_UnlockSlotHandle(slotHandle);
        }
        else if (Result != VHDERR_TIMEOUT)
            throw std::runtime_error("ERROR : Cannot lock slot on RX0 stream.");
    }
}
inline         auto deltaCast::byteCombineToShort(const std::uint8_t* sourceAudio,
                                          const std:: size_t  sourceSize) -> std::vector<short>
{
    const auto newSize = sourceSize / 2;
    std::vector<short> shortArr(newSize);

    if (sourceSize % 2)
        throw std::invalid_argument("HDMI Audio Error : Invalid buffer size for 16 bit audio data.");
    else
        for (auto i{0}, j{0}; i < sourceSize; i += 2, ++j)
            shortArr[j] = static_cast<std::uint32_t>(sourceAudio[i + 1]  << 8) |
                          static_cast<std::uint32_t>(sourceAudio[i    ]);
    
    return shortArr;
}


#pragma endregion
#endif //AUDIOMIXER_DELTACAST_H