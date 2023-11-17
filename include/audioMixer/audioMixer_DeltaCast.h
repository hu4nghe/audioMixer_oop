#ifndef AUDIOMIXER_DELTACAST_H
#define AUDIOMIXER_DELTACAST_H

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Dv.h"
#include "VideoMasterHD_Dv_Audio.h"
#include "audioMixer_base.h"

#include <print>

class boardHandle
{
    std::uint32_t boardID{};
    void*         handle {};
public:
    boardHandle();
   ~boardHandle();
};
boardHandle::boardHandle()
{
    if (VHD_OpenBoardHandle(boardID, &handle, nullptr, 0))
        throw std::runtime_error("Cannot open DELTA board handle.");
    else
    {
        ULONG boardType{};
        VHD_GetBoardProperty(handle, VHD_CORE_BP_BOARD_TYPE, &boardType);
        auto isHDMI = ((boardType == VHD_BOARDTYPE_HDMI)            || 
                       (boardType == VHD_BOARDTYPE_HDMI20)          || 
                       (boardType == VHD_BOARDTYPE_FLEX_HMI)        || 
                       (boardType == VHD_BOARDTYPE_MIXEDINTERFACE));
        if(!isHDMI)
            throw std::runtime_error("ERROR : The selected board is not a HDMI or DVI board");

    }

}
inline boardHandle::~boardHandle()
{
    VHD_CloseBoardHandle(handle);
}

class streamHandle
{
    void* handle;

};

class deltaCast : public audioMixerModule_base
{
    VHD_DV_AUDIO_INFOFRAME HDMIAudioInfoFrame{};
    VHD_DV_AUDIO_AES_STS   HDMIAudioAESSts   {};

    std::vector<short> composition16bit(const std::uint8_t* sourceAudio,
                                        const std:: size_t  sourceSize);
public:
	deltaCast(const outputParameter& outputCfg);

	void start() override;
	void stop () override;

	        void recvAudio        ();
    std:: size_t getSampleRate    () const;
    std::uint8_t getChannelNumbers() const;
    
};
#pragma region IMPL

inline std::vector<short> deltaCast::composition16bit(const std::uint8_t* sourceAudio,
                                                      const std:: size_t  sourceSize)
{
    const auto newSize = sourceSize / 2;
    std::vector<short> combined16bit(newSize);

    if (sourceSize % 2)
        throw std::invalid_argument("HDMI Audio Error : Invalid buffer size for 16 bit audio data.");
    else
        for (auto i = 0, j = 0; i < sourceSize; i += 2, ++j)
            combined16bit[j] = static_cast<std::uint32_t>(sourceAudio[i    ]  << 8) |
                               static_cast<std::uint32_t>(sourceAudio[i + 1]);
    
    return combined16bit;
}

deltaCast::deltaCast(const outputParameter& outputCfg)
	:	module(outputCfg)
{
    ULONG boardNumber{};
    if (VHD_GetApiInfo(nullptr, &boardNumber) == VHDERR_NOERROR)
    {
        if(!boardNumber)
            throw std::runtime_error("No Delta board detected.");
    }
    else
        throw std::runtime_error("Cannot query VideoMaster information.");
}

inline void deltaCast::start()
{

}

inline void deltaCast::stop()
{

}

inline void deltaCast::recvAudio()
{
    {
        ULONG                      Result, dllVersion, boardNumber, boardType;
        ULONG                      i;
        ULONG                      NbFramesRecv = 0, NbFramesDropped = 0, BufferSize = 0, AudioBufferSize = 0, PxlClk = 0;
        HANDLE                     BoardHandle = nullptr, StreamHandle = nullptr, SlotHandle = nullptr;
        ULONG                      Height = 0, Width = 0, RefreshRate = 0;
        VHD_DV_MODE                DvMode = NB_VHD_DV_MODES;
        ULONG                      BrdId = 0;
        BOOL32                     Interlaced = FALSE;
        BOOL32                     IsHdmiBoard = FALSE;
        BYTE*                      pBuffer = nullptr, * pAudioBuffer = nullptr;
        VHD_DV_CS                  InputCS;
        VHD_DV_AUDIO_TYPE          HDMIAudioType;
        
        /* Query VideoMaster information */
        Result = VHD_GetApiInfo(&dllVersion, &boardNumber);
        if (Result == VHDERR_NOERROR)
        {
            std::print("VideoMaster DLL v{0}.{1}.{2}\nboard number : {3}",
                        dllVersion >> 24, 
                       (dllVersion >> 16) & 0xFF, 
                        dllVersion & 0xFFFF,
                        boardNumber);

            if (boardNumber > 0)
            {
                /* Open a handle on selected DELTA board */
                Result = VHD_OpenBoardHandle(BrdId, &BoardHandle, nullptr, 0);
                if (Result == VHDERR_NOERROR)
                {
                    VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_BOARD_TYPE, &boardType);
                    IsHdmiBoard = ((boardType == VHD_BOARDTYPE_HDMI) || (boardType == VHD_BOARDTYPE_HDMI20) || (boardType == VHD_BOARDTYPE_FLEX_HMI) || (boardType == VHD_BOARDTYPE_MIXEDINTERFACE));

                    /* Check the board type of the selected board */
                    if (IsHdmiBoard)
                    {
                        /* Create a logical stream to receive from RX0 connector */
                        Result = VHD_OpenStreamHandle(BoardHandle, VHD_ST_RX0, IsHdmiBoard ? VHD_DV_STPROC_JOINED : VHD_DV_STPROC_DEFAULT, nullptr, &StreamHandle, nullptr);
                        if (Result == VHDERR_NOERROR)
                        {
                            /* Set the primary mode of this channel to HDMI */
                            Result = VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_MODE, VHD_DV_MODE_HDMI);
                            if (Result == VHDERR_NOERROR)
                            {
                                printf("Waiting for incoming HDMI signal... \n");
                                do
                                {
                                    /* Auto-detect Dv mode */
                                    Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_MODE, (ULONG*)&DvMode);

                                } while (DvMode != VHD_DV_MODE_HDMI || Result != VHDERR_NOERROR);

                                if (Result == VHDERR_NOERROR && DvMode == VHD_DV_MODE_HDMI)
                                {
                                    /* Configure RGBA reception (no color-space conversion) */
                                    VHD_SetStreamProperty(StreamHandle, VHD_CORE_SP_BUFFER_PACKING, VHD_BUFPACK_VIDEO_RGB_32);
                                    VHD_SetStreamProperty(StreamHandle, VHD_CORE_SP_TRANSFER_SCHEME, VHD_TRANSFER_SLAVED);

                                    /* Get auto-detected resolution */
                                    Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_ACTIVE_WIDTH, &Width);
                                    if (Result == VHDERR_NOERROR)
                                    {
                                        Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_ACTIVE_HEIGHT, &Height);
                                        if (Result == VHDERR_NOERROR)
                                        {
                                            Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_INTERLACED, (ULONG*)&Interlaced);
                                            if (Result == VHDERR_NOERROR)
                                            {
                                                Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_REFRESH_RATE, &RefreshRate);
                                                if (Result == VHDERR_NOERROR)
                                                {
                                                    if (IsHdmiBoard)
                                                        Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_INPUT_CS, (ULONG*)&InputCS);

                                                    if (Result == VHDERR_NOERROR)
                                                    {
                                                        if (IsHdmiBoard)
                                                            Result = VHD_GetStreamProperty(StreamHandle, VHD_DV_SP_PIXEL_CLOCK, &PxlClk);

                                                        if (Result == VHDERR_NOERROR)
                                                        {
                                                            std::print("Incoming graphic resolution : %ux%u (%s)\n", Width, Height, Interlaced ? "Interlaced" : "Progressive");

                                                            /* Configure stream. Only VHD_DV_SP_ACTIVE_WIDTH, VHD_DV_SP_ACTIVE_HEIGHT,
                                                            VHD_DV_SP_INTERLACED and VHD_DV_SP_REFRESH_RATE properties are required for HDMI */

                                                            VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_ACTIVE_WIDTH, Width);
                                                            VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_ACTIVE_HEIGHT, Height);
                                                            VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_INTERLACED, Interlaced);
                                                            VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_REFRESH_RATE, RefreshRate);

                                                            /* VHD_DV_SP_INPUT_CS and VHD_DV_SP_PIXEL_CLOCK are required for DELTA-h4k only */
                                                            if (boardType == VHD_BOARDTYPE_HDMI)
                                                            {
                                                                VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_INPUT_CS, InputCS);
                                                                VHD_SetStreamProperty(StreamHandle, VHD_DV_SP_PIXEL_CLOCK, PxlClk);
                                                            }

                                                            /* Start stream */
                                                            Result = VHD_StartStream(StreamHandle);
                                                            if (Result == VHDERR_NOERROR)
                                                            {
                                                                std::print("Reception started\n");

                                                                /* Reception loop */
                                                                while (true)
                                                                {
                                                                    /* Try to lock next slot */
                                                                    Result = VHD_LockSlotHandle(StreamHandle, &SlotHandle);

                                                                    if (Result == VHDERR_NOERROR)
                                                                    {
                                                                        Result = VHD_GetSlotBuffer(SlotHandle, VHD_DV_BT_VIDEO, &pBuffer, &BufferSize);
                                                                        if (Result == VHDERR_NOERROR)
                                                                        {
                                                                            /* Do RX data processing here on pBuffer */
                                                                        }
                                                                        else
                                                                        {
                                                                            std::print("ERROR : Cannot get video slot buffer.\n");
                                                                            break;
                                                                        }
                                                                        if (IsHdmiBoard)
                                                                        {
                                                                            /* Do RX data processing here on pBuffer */
                                                                            Result = VHD_GetSlotDvAudioInfo(SlotHandle, &HDMIAudioType, &HDMIAudioInfoFrame, &HDMIAudioAESSts);

                                                                            if (Result == VHDERR_NOERROR)
                                                                            {
                                                                                if (HDMIAudioType != VHD_DV_AUDIO_TYPE_NONE)
                                                                                {
                                                                                    BufferSize = 0;
                                                                                    if (HDMIAudioAESSts.LinearPCM == VHD_DV_AUDIO_AES_SAMPLE_STS_LINEAR_PCM_SAMPLE)
                                                                                    {
                                                                                        VHD_SlotExtractDvPCMAudio(SlotHandle, VHD_DVAUDIOFORMAT_16, 0x3, nullptr, &BufferSize);
                                                                                        pAudioBuffer = new UBYTE[BufferSize];
                                                                                        Result = VHD_SlotExtractDvPCMAudio(SlotHandle, VHD_DVAUDIOFORMAT_16, 0x3, pAudioBuffer, &BufferSize);
                                                                                        if (Result == VHDERR_NOERROR)
                                                                                        {
                                                                                            auto temp = composition16bit(pAudioBuffer, BufferSize);
                                                                                            std::vector<float> floatData(temp.size());
                                                                                            
                                                                                            src_short_to_float_array(temp.data(), floatData.data(), temp.size());

                                                                                            (*audio)[0].push(std::move(floatData), getSampleRate(), getChannelNumbers());
                                                                                            
                                                                                        }
                                                                                        else
                                                                                        {
                                                                                            std::print("ERROR : Cannot get PCM audio slot buffer. \n");
                                                                                            break;
                                                                                        }
                                                                                    }
                                                                                    else
                                                                                    {
                                                                                        VHD_SlotExtractDvNonPCMAudio(SlotHandle, nullptr, &BufferSize);
                                                                                        pAudioBuffer = new UBYTE[BufferSize];
                                                                                        Result = VHD_SlotExtractDvNonPCMAudio(SlotHandle, pAudioBuffer, &BufferSize);
                                                                                        if (Result == VHDERR_NOERROR)
                                                                                        {
                                                                                            /* Do Non-PCM audio data processing here on pAudioBufferPacked_UB */
                                                                                        }
                                                                                        else
                                                                                        {
                                                                                            std::print("ERROR : Cannot get Non-PCM audio slot buffer. \n");
                                                                                            break;
                                                                                        }
                                                                                    }

                                                                                    if (pAudioBuffer)
                                                                                    {
                                                                                        delete[] pAudioBuffer;
                                                                                        pAudioBuffer = nullptr;
                                                                                    }
                                                                                }
                                                                            }
                                                                            else
                                                                            {
                                                                                std::print("ERROR : Cannot get HDMI audio info. \n");
                                                                                break;
                                                                            }
                                                                        }

                                                                        /* Unlock slot */
                                                                        VHD_UnlockSlotHandle(SlotHandle);

                                                                        /* Print some statistics */
                                                                        VHD_GetStreamProperty(StreamHandle, VHD_CORE_SP_SLOTS_COUNT, &NbFramesRecv);
                                                                        VHD_GetStreamProperty(StreamHandle, VHD_CORE_SP_SLOTS_DROPPED, &NbFramesDropped);
                                                                        printf("%u frames received (%u dropped)            \r", NbFramesRecv, NbFramesDropped);
                                                                        fflush(stdout);
                                                                    }
                                                                    else if (Result != VHDERR_TIMEOUT)
                                                                    {
                                                                        std::print("ERROR : Cannot lock slot on RX0 stream. \n");
                                                                        break;
                                                                    }
                                                                }

                                                                std::print("");

                                                                /* Stop stream */
                                                                VHD_StopStream(StreamHandle);
                                                            }
                                                            else
                                                                printf("ERROR : Cannot start RX0 stream on DV board handle. \n");
                                                        }
                                                        else
                                                            printf("ERROR : Cannot detect incoming pixel clock from RX0. \n");
                                                    }
                                                    else
                                                        printf("ERROR : Cannot detect incoming color space from RX0. \n");
                                                }
                                                else
                                                    printf("ERROR : Cannot detect incoming refresh rate from RX0. \n");
                                            }
                                            else
                                                printf("ERROR : Cannot detect if incoming stream from RX0 is interlaced or progressive. \n");
                                        }
                                        else
                                            printf("ERROR : Cannot detect incoming active height from RX0. \n");
                                    }
                                    else
                                        printf("ERROR : Cannot detect incoming active width from RX0. \n");
                                }
                                else
                                    printf("ERROR : Cannot detect if incoming mode is HDMI. \n");
                            }
                            else
                                printf("ERROR : Cannot configure RX0 stream primary mode. \n");

                            /* Close stream handle */
                            VHD_CloseStreamHandle(StreamHandle);
                        }
                        else
                            printf("ERROR : Cannot open RX0 stream on HDMI / DVI board handle. \n");
                    }
                    else
                        printf("ERROR : The selected board is not a HDMI or DVI board\n");

                    /* Close board handle */
                    VHD_CloseBoardHandle(BoardHandle);
                }
                else
                    printf("ERROR : Cannot open DELTA board handle.\n");
            }
            else
                printf("No DELTA board detected, exiting...\n");
        }
        else
            printf("ERROR : Cannot query VideoMaster information. \n");
    }
}

inline std::size_t deltaCast::getSampleRate() const 
{
    const int code = HDMIAudioInfoFrame.SamplingFrequency;
    std::size_t sampleRate{};
    switch (code)
    {
    case VHD_DV_AUDIO_INFOFRAME_SAMPLING_FREQ_REF_STREAM_HEADER:
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

inline std::uint8_t deltaCast::getChannelNumbers() const
{
    std::uint8_t channelNumber{};
    const int code = HDMIAudioInfoFrame.ChannelCount;
    switch (code) {
    case VHD_DV_AUDIO_INFOFRAME_CHANNEL_COUNT_REF_STREAM_HEADER:
        channelNumber = HDMIAudioAESSts.ChannelNb;
        break;
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
    return channelNumber
}

#pragma endregion
#endif //AUDIOMIXER_DELTACAST_H