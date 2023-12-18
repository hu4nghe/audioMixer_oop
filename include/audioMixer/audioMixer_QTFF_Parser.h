#ifndef AUDIOMIXER_QTFF_PARSER_H
#define AUDIOMIXER_QTFF_PARSER_H

#include <filesystem>
#include <iostream>
#include <fstream>
#include <bitset>

#include "audioMixer_base.h"
#include <variant>

namespace fs = std::filesystem;

constexpr std::uint32_t ftypCode = 0x66747970;
constexpr std::uint32_t moovCode = 0x6D6F6F76;
constexpr std::uint32_t trakCode = 0x7472616B;
constexpr std::uint32_t sounCode = 0x736F756E;

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
        std::swap(reinterpret_cast<uint8_t*>(&value)[i], reinterpret_cast<uint8_t*>(&value)[sizeof(T) - 1 - i]);
    }
};

using sPtrFile = std::shared_ptr<fileReader>;

static std::string getStr(std::uint32_t code)
{
    std::bitset<32> bits(code);
    std::string result;
    for (int i = 24; i >= 0; i -= 8)
        result += static_cast<char>((bits >> i).to_ulong() & 0xFF);
    return result;
}

class atom
{
protected:
	std::uint32_t  atomSize;
	std::uint32_t  atomType;
    std::  size_t  posBegin;
         sPtrFile  file;
public:
    atom() = default;
    atom(sPtrFile fileStream)
    {
        file = fileStream;
        try
        {
            posBegin = file->tellg();
            file->readBigEndian(atomSize);
            file->readBigEndian(atomType);
            std::print("atomType : {}\nsize : {}\n", getStr(atomType), atomSize);
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
    sPtrFile fileStream;
    std::vector<atom> soundTracksMdia;
    sPtrQueueList<float> audio;
public:
    QTFF(const std::string& filePath)
    {
        try
        {
            fileStream = std::make_shared<fileReader>();
            fs::path qtffPath(filePath);
            if (!fs::exists(qtffPath))
                throw std::runtime_error("Invalid file path.");
            fileStream->open(filePath, std::ios::binary);
            if (!fileStream->is_open())
                throw std::runtime_error("Failed to open file.");

            atom fileType(fileStream);
            if (!fileType.typeIs("ftyp"))
                throw std::runtime_error("Not a valid QTFF file.");
            fileType.skip();
            //audio = std::make_shared<std::vector<audioQueue<float>>>(outputCfg);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::format("Failed to parse file : {}.", e.what()));
        }
    }
   ~QTFF() { fileStream->close(); }
    atom searchAtom     (std::string_view code)
    {
        fileStream->seekg(0, std::ios::end);
        std::size_t eof = fileStream->tellg();
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
        return atom();
    }
    void searchAudioInfo()
    {
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
        for (auto& mdia : soundTracksMdia)
        {
            fileStream->seekg(mdia.head(), std::ios::beg);
            auto minfAtom = searchAtom("minf");
            auto stblAtom = searchAtom("stbl");
            auto stsdAtom = searchAtom("stsd");
            
            /*
             * stsd atom structure
             * data fields            atomSize    pos   status          diff
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
            std::uint32_t nbentries = 0;
            fileStream->readBigEndian(nbentries);

            for (auto i = 0; i < nbentries; i++)
            {
                auto mp4aAtom = searchAtom("mp4a");
                /*
                 * mp4a layout
                 * data fields            atomSize    pos   status          diff
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
                std::uint16_t channelCount = 0;
                fileStream->seekg(16, std::ios::cur);//seek target(1)
                fileStream->readBigEndian(channelCount);
                std::print("channelNum : {}\n", channelCount);

                //target(2)
                std::uint16_t sampleSize = 0;
                fileStream->readBigEndian(sampleSize);
                std::print("sampleSize : {}\n", sampleSize);

                std::uint32_t sampleRate = 0;
                fileStream->seekg(2, std::ios::cur);//seek target(3)
                fileStream->readBigEndian(sampleRate);
                std::print("sampleRate : {}\n", sampleRate);
                
            }
            //mp4aAtom.skip();
            break;
        }
    }
    void findSoundTrack(atom& track)
    {
        if (getStr(track.type()) != "trak")
            throw std::invalid_argument("Not a trak atom.");
        auto mdiaAtom = searchAtom("mdia");
        auto hdlrAtom = searchAtom("hdlr");
        /*
         * hdlr atom structure
         * data fields            atomSize    pos   status      diff
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
            soundTracksMdia.push_back(mdiaAtom);
        track.skip();
    }
};

#endif