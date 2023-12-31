#ifndef AUDIOMIXER_QTFF_PARSER_H
#define AUDIOMIXER_QTFF_PARSER_H

#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

extern "C" 
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#include "audioMixer_base.h"
#include <variant>

namespace fs = std::filesystem;

constexpr std::uint32_t ftypCode = 0x66747970;
constexpr std::uint32_t moovCode = 0x6D6F6F76;
constexpr std::uint32_t trakCode = 0x7472616B;
constexpr std::uint32_t sounCode = 0x736F756E;

static std::string getStr(std::uint32_t code)
{
    std::bitset<32> bits(code);
    std::string result;
    for (std::int32_t i = 24; i >= 0; i -= 8)
        result += static_cast<char>((bits >> i).to_ulong() & 0xFF);
    return result;
}

class atomNotFound : public modObjNotFound
{
public: 
    atomNotFound(const std::string msg) : modObjNotFound(msg) {}
    const char* what() const noexcept override { return modObjNotFound::what(); }
};

class fileReader : public std::ifstream
{
public:
    fileReader& readBigEndian(char* buffer, std::streamsize count) 
    {
        if (!this->good()) 
            throw std::ios_base::failure("Error reading from file.");
        try 
        {
            std::streamsize num = this->rdbuf()->sgetn(buffer, count);

            for (std::streamsize i = 0; i < count / 2; ++i)
                std::swap(buffer[i], buffer[count - 1 - i]);
            if (num != count) 
            {
                this->setstate(std::ios_base::eofbit | std::ios_base::failbit);
                throw std::ios_base::failure("Error reading from file.");
            }
        }
        catch (...) 
        {
            this->setstate(std::ios_base::badbit | std::ios_base::failbit);
            throw;  // Re-throw the caught exception
        }

        return *this;
    }

    template<typename T>
    void readBigEndian(T& value)
    {
        this->read(reinterpret_cast<char*>(&value), sizeof(T));
        for (size_t i = 0; i < sizeof(T) / 2; ++i)
        std::swap(reinterpret_cast<std::uint8_t*>(&value)[i], 
                  reinterpret_cast<std::uint8_t*>(&value)[sizeof(T) - 1 - i]);
    }
};

class atom
{
protected:
	std::uint32_t     atomSize;
	std::uint32_t     atomType;
    std::  size_t     posBegin;
    sPtr<fileReader>  file;
public:
    atom(sPtr<fileReader> fileStream)
    {
        file = fileStream;
        try
        {
            posBegin = file->tellg();
            file->readBigEndian(atomSize);
            file->readBigEndian(atomType);
            //std::print("atomType : {}\nsize : {}\n", getStr(atomType), atomSize);
        }
        catch (const std::exception& e)
        {
            std::print(stderr, "Error : {}.\n", e.what());
            throw std::runtime_error("Failed to read file.");
        }
    }
   ~atom() = default;
    
    void restart()
    {
        file->seekg(posBegin);
    }
    void skip()
    {
        restart();
        file->seekg(atomSize, std::ios::cur);
    }
    bool typeIs (std::string_view fourCC) const { return fourCC == getStr(atomType); }

    [[nodiscard]] std::uint32_t type   () const noexcept{ return atomType; }
    [[nodiscard]] std::uint32_t size   () const noexcept{ return atomSize; }
    [[nodiscard]] std::uint32_t head   () const noexcept{ return posBegin; }
    [[nodiscard]] std::uint32_t end    () const noexcept{ return posBegin + atomSize; }
};

class QTFF
{
    sPtr<fileReader> fileStream;
    std::vector<atom> soundTracksMdia;
    std::vector<std::uint8_t> aacData;
    std::size_t eof;
public:
    QTFF(const fs::path& qtffFile, const outputParameter& outputConfig)
    {
        try
        {
            fileStream = std::make_shared<fileReader>();
            fileStream->open(qtffFile.c_str(), std::ios::binary);
            if (!fileStream->is_open())
                throw std::invalid_argument(std::format("Failed to open file : {}\n", qtffFile.string()));
            
            fileStream->seekg(0, std::ios::end);
            eof = fileStream->tellg();
            fileStream->seekg(0, std::ios::beg);

            atom fileType(fileStream);
            if (!fileType.typeIs("ftyp"))
                throw std::invalid_argument("Not a valid QTFF file.");
            fileType.skip();
        }
        catch (const std::invalid_argument& e)
        {
            throw std::runtime_error(std::format("Failed to parse file : {}.", e.what()));
        }
    }
   ~QTFF() { fileStream->close(); }

    atom searchAtom    (const std::string& code) const
    {
        bool exit = false;
        while(!exit)
        {
            atom header(fileStream);
            if (header.typeIs(code))
                return header;
            else
                header.skip();
            if (header.end() == eof)
                exit = true;
        }
        throw atomNotFound(code);
    }
    void findSoundTrack(atom& track)
    {
        if (getStr(track.type()) != "trak")
            throw std::invalid_argument("Not a trak atom.");
        auto mdiaAtom = searchAtom("mdia");
        auto hdlrAtom = searchAtom("hdlr");
        /*
         * hdlr atom structure
         * data fields            atomSize    pos   status      diffSeekg
         * ----------------------------------------------------------
         * Size                   4       0     read
         * Type                   4       4     read
         * -----------------------------> 8     current pos
         * Version                1       8     ignore
         * Flags                  3       9     ignore
         * Component atomType     4       12    ignore
         * Conponent subtype      4       16    target          8
         * ...
         */
        std::uint32_t trackType = 0;
        fileStream->seekg(8, std::ios::cur);
        fileStream->readBigEndian(trackType);
        if (getStr(trackType) == "soun")
        {
            hdlrAtom.skip();
            auto minfAtom = searchAtom("minf");
            auto stblAtom = searchAtom("stbl");
            soundTracksMdia.push_back(stblAtom);
        }
        track.skip();
    }
    void searchAudioInfo()
    {
        try
        {
            std::uint16_t channelCount = 0;
            std::uint16_t bitDepth = 0;
            std::uint32_t sampleRate = 0;

            auto moovAtom = searchAtom("moov");
            bool exit = false;
            //parse all tracks
            while (!exit)
            {
                try
                {
                    auto trakAtom = searchAtom("trak");
                    if (trakAtom.size() == 0)
                        throw std::runtime_error("track no found.");
                    else
                        findSoundTrack(trakAtom);
                }
                catch (const std::exception& e)
                {
                    exit = true;
                }
            }
            for (auto& stbl : soundTracksMdia)
            {
                //Read offset : head of stbl + 4 byte size + 4 bit type.
                std::size_t stblHeadOffset = static_cast<size_t>(stbl.head()) + 8;
                fileStream->seekg(stblHeadOffset, std::ios::beg);

                auto stsdAtom = searchAtom("stsd");
                /*
                 * stsd atom structure
                 * data fields            atomSize    pos   status      diffSeekg
                 * ----------------------------------------------------------
                 * Size                   4       0     read
                 * Type                   4       4     read
                 * -----------------------------> 8     current pos
                 * Version                1       8     ignore
                 * Flags                  3       9     ignore
                 * Number of entries      4       12    target(1)       4
                 * ...
                 */
                fileStream->seekg(4, std::ios::cur);
                std::uint32_t nbEntriesStsd = 0;
                fileStream->readBigEndian(nbEntriesStsd);

                for (auto i = 0; i < nbEntriesStsd; i++)
                {
                    auto mp4aAtom = searchAtom("mp4a");
                    /*
                     * mp4a layout
                     * data fields            atomSize    pos   status      diffSeekg
                     * ----------------------------------------------------------
                     * Size                   4       0     read
                     * Type                   4       4     read
                     * -----------------------------> 8     current pos
                     * Reserved               6       8     ignore
                     * Data Reference Index   2       14    ignore
                     * Reserved               8       16    ignore
                     * Channel Count          2       24    target(1)       16
                     * -----------------------------> 26    pos after(1)
                     * Sample Size            2       26    target(2)       0
                     * -----------------------------> 28    pos after(2)
                     * Reserved               2       28    ignore
                     * Sample Rate            4       30    target(3)       2
                     */
                    
                    fileStream->seekg(16, std::ios::cur);//seek target(1)
                    fileStream->readBigEndian(channelCount);
                    std::print("channel number : {}\n", channelCount);

                    //target(2)
                    
                    fileStream->readBigEndian(bitDepth);
                    std::print("bit depth      : {}\n", bitDepth);

                    
                    fileStream->seekg(2, std::ios::cur);//seek target(3)
                    fileStream->readBigEndian(sampleRate);
                    std::print("sample rate    : {}\n\n", sampleRate);
                }
                stsdAtom.skip();

                auto stszAtom = searchAtom("stsz");
                /*
                 * stsz atom structure
                 * data fields            atomSize    pos   status      diffSeekg
                 * ----------------------------------------------------------
                 * Size                   4       0     read
                 * Type                   4       4     read
                 * -----------------------------> 8     current pos
                 * Version                1       8     ignore
                 * Flags                  3       9     ignore
                 * Sample size            4       12    target(1)       4
                 * Number of entries      4       16    target(2)       0
                 * Sample size table      4*n     20    target(3)       0
                 * ...
                 */
                fileStream->seekg(4, std::ios::cur);
                std::uint32_t sampleSize = 0;
                fileStream->readBigEndian(sampleSize);

                bool uniqueSampleSize = true;
                std::vector<uint32_t> stszTable;

                if (sampleSize == 0)//sizes are stored in the sample size table.
                {
                    uniqueSampleSize = false;
                    std::uint32_t nbEntriesStsz = 0;
                    fileStream->readBigEndian(nbEntriesStsz);
                    stszTable.reserve(nbEntriesStsz);
                    std::uint32_t size = 0;
                    for (auto i = 0; i < nbEntriesStsz; i++)
                    {
                        fileStream->readBigEndian(size);
                        stszTable.push_back(size);
                    }
                }
                stszAtom.skip();

                auto stcoAtom = searchAtom("stco");
                /*
                 * stco atom structure
                 * data fields            atomSize    pos   status      diffSeekg
                 * ----------------------------------------------------------
                 * Size                   4       0     read
                 * Type                   4       4     read
                 * -----------------------------> 8     current pos
                 * Version                1       8     ignore
                 * Flags                  3       9     ignore
                 * Number of entries      4       12    target(1)       4
                 * Sample size table      4*n     16    target(2)       0
                 */
                fileStream->seekg(4, std::ios::cur);
                std::uint32_t nbChunk = 0;
                fileStream->readBigEndian(nbChunk);
                std::vector<uint32_t> stcoTable(nbChunk);

                std::uint32_t chunkOffset = 0;
                for (auto i = 0; i < nbChunk; i++)
                {
                    fileStream->readBigEndian(chunkOffset);
                    stcoTable[i] = chunkOffset;
                }

                //go back to read stsc atom
                fileStream->seekg(stbl.head() + 8, std::ios::beg);
                auto stscAtom = searchAtom("stsc");
                /*
                 * stsc atom structure
                 * data fields            atomSize    pos   status      diffSeekg
                 * ----------------------------------------------------------
                 * Size                   4       0     read
                 * Type                   4       4     read
                 * -----------------------------> 8     current pos
                 * Version                1       8     ignore
                 * Flags                  3       9     ignore
                 * Number of entries      4       12    target(1)       4
                 * Sample-to-chunk table  4*3*n   16    target(2)       0
                 */

                std::uint32_t nbEntriesStsc = 0;
                fileStream->seekg(4, std::ios::cur);
                fileStream->readBigEndian(nbEntriesStsc);
                std::uint32_t temp = 0;

                std::vector<std::uint32_t> firstChunk(nbEntriesStsc);
                std::vector<std::uint32_t> samplesPerChunk(nbEntriesStsc);
                std::vector<std::uint32_t> sampleDescriptionID(nbEntriesStsc);
                std::vector<std::uint32_t> completeTable;
                completeTable.reserve(nbChunk);

                for (auto i = 0; i < nbEntriesStsc; i++)
                {
                    fileStream->readBigEndian(temp);
                    firstChunk[i] = temp;
                    fileStream->readBigEndian(temp);
                    samplesPerChunk[i] = temp;
                    fileStream->readBigEndian(temp);
                    sampleDescriptionID[i] = temp;
                }

                for (std::size_t i = 0; i < nbEntriesStsc; i++)
                {
                    if (i == 0)
                    {
                        completeTable.push_back(samplesPerChunk[i]);
                        continue;
                    }
                    else
                    {
                        auto diff = firstChunk[i] - firstChunk[i - 1];
                        if (diff > 1)
                            for (std::size_t j = 1; j < diff; j++)
                                completeTable.push_back(samplesPerChunk[i - 1]);
                        completeTable.push_back(samplesPerChunk[i]);
                    }
                }
                stscAtom.skip();
                
                //start of audio data extraction
                std::uint8_t sampleTemp      = 0;
                std:: size_t sampleReadCount = 0;
                
                for (auto&& [chunkOfst, samplePerChunk] : std::views::zip(stcoTable, completeTable))
                {
                    fileStream->seekg(chunkOfst, std::ios::beg);
                    for (auto i = 0; i < samplePerChunk; i++)
                    {
                        for (auto j = 0; j < stszTable[sampleReadCount]; j++)
                        {
                            fileStream->readBigEndian(sampleTemp);
                            aacData.push_back(sampleTemp); 
                        }
                        sampleReadCount++;
                    }
                }
                std::print("aac Data total size : {} byte.\n", aacData.size());
                decodeAAC(sampleRate, channelCount, bitDepth);
                break;
            }
        }
        catch (const atomNotFound& err)
        {
            std::print("File parsing failed : {} atom not found.",err.what());
            throw;
        }
    }
    void decodeAAC(const std::int32_t sampleRate,const std::int32_t channelNumber,const std::int32_t bitDepth)
    {
        AVFormatContext* formatContext = avformat_alloc_context();
        const auto aacCodec = avcodec_find_decoder(AV_CODEC_ID_AAC);
              auto codecCfg = avcodec_alloc_context3(aacCodec);
        codecCfg->codec_id  = AV_CODEC_ID_AAC;
        if(avcodec_open2(codecCfg, aacCodec, nullptr)) 
        {
            avcodec_free_context(&codecCfg);
            throw modFatalErr("ffmpeg : fail to open codec.");
        }

        //emballage avpacket
        auto packet = av_packet_alloc();
        packet->data = aacData.data();
        packet->size = aacData.size();

        AVFrame* frame = av_frame_alloc();
        while (av_read_frame(formatContext, packet) >= 0)
        {
            if (packet->stream_index == 0)
            {
                AVFrame* frame = av_frame_alloc();
                if (!frame)
                {
                    std::cerr << "Failed to allocate frame." << std::endl;
                    break;
                }
            }
        }
        auto res = avcodec_receive_frame(codecCfg, frame);
        std::print("{}\n", res);
        if (res < 0 && res != AVERROR(EAGAIN) && res != AVERROR_EOF)
        {
            av_frame_free(&frame);
            avcodec_free_context(&codecCfg);
            throw modFatalErr("Codec processing error.");
        }
        std::print("Parameters after decode :\n");
        std::print("format:{}\n",frame->format);
        std::print("sample rate : {}\n", frame->sample_rate);
        
    }
};

#endif

