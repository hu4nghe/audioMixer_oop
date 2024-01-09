#ifndef AUDIOMIXER_DELTACAST_H
#define AUDIOMIXER_DELTACAST_H

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Dv.h"
#include "VideoMasterHD_Dv_Audio.h"

#include "audioMixer_base.h"

#pragma region Handles
class handle
{
protected:
    void* hdl = nullptr;
public:
    handle() = default;

    virtual ~handle() {};
    [[nodiscard]] auto getHandle() const { return hdl; }
};

class boardHandle : public handle
{
    std::uint32_t boardID = 0;
public:
    boardHandle()
    {
        if (VHD_OpenBoardHandle(boardID, &hdl, nullptr, 0))
            throw modFatalErr("Cannot open DELTA board handle.");
        else
        {
            ULONG boardType = 0;
            VHD_GetBoardProperty(hdl, VHD_CORE_BP_BOARD_TYPE, &boardType);
            bool isHDMI = ((boardType == VHD_BOARDTYPE_HDMI)            ||
                           (boardType == VHD_BOARDTYPE_HDMI20)          ||
                           (boardType == VHD_BOARDTYPE_FLEX_HMI)        ||
                           (boardType == VHD_BOARDTYPE_MIXEDINTERFACE));
            if (!isHDMI)
                throw modFatalErr("Delta Cast error : The selected board is not a HDMI or DVI board");
        }
    }
   ~boardHandle(){ VHD_CloseBoardHandle(hdl); }
};

class streamHandle : public handle
{
public: 
    streamHandle(const boardHandle& brdHdl)
    {
        if (VHD_OpenStreamHandle(brdHdl.getHandle(), VHD_ST_RX0, VHD_DV_STPROC_JOINED, nullptr, &hdl, nullptr))
            throw modFatalErr("Delta Cast error : Cannot open RX0 stream on HDMI / DVI board handle.");
        else
            if (VHD_SetStreamProperty(hdl, VHD_DV_SP_MODE, VHD_DV_MODE_HDMI))
                throw modFatalErr("Delta Cast error : Cannot configure RX0 stream primary mode.");
    }
   ~streamHandle(){ VHD_CloseStreamHandle(hdl); }
};
#pragma endregion

class deltaCast : public audioMixerModule_base
{
    //handles
    std::unique_ptr< boardHandle> boardHdl;
    std::unique_ptr<streamHandle> streamHdl;
    //HDMI audio infos
    VHD_DV_AUDIO_INFOFRAME HDMIAudioInfoFrame{};
    VHD_DV_AUDIO_AES_STS   HDMIAudioAESSts   {};

    auto byteCombineToShort(const std::uint8_t* sourceAudio,
                            const std:: size_t  sourceSize) -> std::vector<short>;
public:
	deltaCast(const outputParameter& outputCfg);

	void start            () override;
	void stop             () override;
    void streamConfig     ();
    void startStream      ();
    auto getSampleRate    () const;
    auto getChannelNumbers() const;
    
};

#pragma region IMPL

            deltaCast::deltaCast         (const outputParameter& outputCfg)
	:	module(outputCfg)
{
    try
    {
        ULONG boardNumber = 0;
        if (VHD_GetApiInfo(nullptr, &boardNumber))
            throw modFatalErr("Cannot query VideoMaster information.");
        else
        {
            if (boardNumber)
            {
                boardHdl = std::make_unique< boardHandle>();
                streamHdl = std::make_unique<streamHandle>(*boardHdl);
            }
            else
                throw modFatalErr("No Delta board detected.");
        }
    }
    catch (const modFatalErr& fatalErr)
    {
        std::print("Delta Cast fatal error : {}\n", fatalErr.what());
        throw fatalErr;
    }
}
inline auto deltaCast::getSampleRate     () const
{
    try
    {
        const std::int32_t code = HDMIAudioInfoFrame.SamplingFrequency;
              std:: size_t sampleRate = 0;
        switch (code)
        {
        case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_REF_STREAM_HEADER:
        {
            //We need to do a reference to stream header.
            const int codeRef = HDMIAudioAESSts.SamplingFrequency;
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
    catch (const std::invalid_argument& err)
    {
        std::print("DeltaCast Module Error : {}\n",err.what());
    }
}
inline auto deltaCast::getChannelNumbers () const
{
    try
    {
        const std::uint32_t code = HDMIAudioInfoFrame.ChannelCount;
              std:: uint8_t channelNumber = 0;

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
    catch (const std::invalid_argument& err)
    {
        std::print("DeltaCast Module Error : {}\n", err.what());
    }
}
inline void deltaCast::start             ()
{
    try
    {
        this->streamConfig();
        this->startStream();
    }
    catch (const modFatalErr& fatalErr)
    {
        std::print("{}\n", fatalErr.what());
        std::print("Q(uit)  R(estart)\n");
        char ch {};
        std::cin >> ch;
        switch (std::toupper(ch))
        {
        case 'Q':
            stop();
            break;
        case 'R':
            stop();
            start();
            break;
        default:
            std::print("invalide choice!\n");
        }
        start();
    }
}
inline void deltaCast::stop              ()
{
    VHD_StopStream(streamHdl->getHandle());
    active = false;
}
inline void deltaCast::streamConfig      ()
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
        ULONG  height      = 0, 
               width       = 0, 
               refreshRate = 0, 
               pxlClk      = 0;
        BOOL32 interlaced  = 0;

        VHD_DV_CS InputCS{};
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_WIDTH ,         &width);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_HEIGHT,         &height);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INTERLACED   , (ULONG*)&interlaced);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_REFRESH_RATE ,         &refreshRate);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INPUT_CS     , (ULONG*)&InputCS);
        VHD_GetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_PIXEL_CLOCK  ,         &pxlClk);
        /* Configure stream.*/
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_WIDTH , width);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_ACTIVE_HEIGHT, height);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INTERLACED   , interlaced);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_REFRESH_RATE , refreshRate);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_INPUT_CS     , InputCS);
        VHD_SetStreamProperty(streamHdl->getHandle(), VHD_DV_SP_PIXEL_CLOCK  , pxlClk);
    }
    else
        throw modFatalErr("Delta Cast error :  Cannot detect if incoming mode is HDMI.");
}
inline void deltaCast::startStream       ()
{
    
    HANDLE slotHandle   = nullptr;
    BYTE*  pBuffer      = nullptr;
    BYTE*  pAudioBuffer = nullptr;
    ULONG  bufferSize   = 0;

    audio->push_back(audioQueue<float>(outputConfig));

    /* Start stream */
    VHD_StartStream(streamHdl->getHandle());
    std::print("HDMI audio reception started\n");
    active = true;
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

                    (*audio)[0].push(std::move(floatData), getSampleRate(), getChannelNumbers());
                    delete[] pAudioBuffer;
                }
            }
            /* Unlock slot */
            VHD_UnlockSlotHandle(slotHandle);
        }
        else if (Result != VHDERR_TIMEOUT)
            throw modFatalErr("Delta Cast error :  Cannot lock slot on RX0 stream.");
    }
}
inline auto deltaCast::byteCombineToShort(const std::uint8_t* sourceAudio,
                                          const std:: size_t  sourceSize) -> std::vector<short>
{
    const auto newSize = sourceSize / 2;
    std::vector<short> shortArr(newSize);

    if (sourceSize % 2)
        throw std::invalid_argument("HDMI Audio Error : Invalid buffer size for 16 bit audio data.");
    else
        for (auto i = 0, j = 0; i < sourceSize; i += 2, ++j)
            shortArr[j] = static_cast<std::uint32_t>(sourceAudio[i + 1]  << 8) |
                          static_cast<std::uint32_t>(sourceAudio[i    ]);
    
    return shortArr;
}
#pragma endregion
//IMPL Delta Cast Module

#endif //AUDIOMIXER_DELTACAST_H