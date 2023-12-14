#ifndef AUDIOMIXER_QTFF_PARSER_H
#define AUDIOMIXER_QTFF_PARSER_H

#include <filesystem>
#include <iostream>
#include <fstream>
#include <bitset>

#include "audioMixer_base.h"

namespace fs = std::filesystem;

constexpr std::uint32_t ftypCode = 0x66747970;
constexpr std::uint32_t moovCode = 0x6D6F6F76;
constexpr std::uint32_t trakCode = 0x7472616B;
constexpr std::uint32_t sounCode = 0x736F756E;

using sPtrFile = std::shared_ptr<std::ifstream>;

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
	std::uint32_t  size;
	std::uint32_t  type;
    std::size_t    posBegin;
    sPtrFile       file;
private:
    std::uint32_t read32bit_bigEndian()
    {
        std::uint32_t rawData = 0;
        file->read(reinterpret_cast<char*>(&rawData), sizeof(std::uint32_t));
        rawData = ((rawData & 0xFF) << 24) | (((rawData >> 8) & 0xFF) << 16) | (((rawData >> 16) & 0xFF) << 8) | ((rawData >> 24) & 0xFF);
        return rawData;
    }
public:
    atom(sPtrFile fileStream)
    {
        file = fileStream;
        try
        {
            posBegin = file->tellg();
            size = read32bit_bigEndian();
            type = read32bit_bigEndian();
            std::print("type : {}\nsize : {}\n", this->getTypeStr(), size);
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
        file->seekg(size, std::ios::cur);
    }
    bool typeIs (std::string_view fourCC) const { return fourCC == this->getTypeStr(); }

    [[nodiscard]] std::uint32_t getType   () const noexcept{ return type; }
    [[nodiscard]] std::uint32_t getSize   () const noexcept{ return size; }
    [[nodiscard]] std::uint32_t getHead   () const noexcept{ return posBegin; }
    [[nodiscard]] std::uint32_t getEnd    () const noexcept{ return posBegin + size; }
    [[nodiscard]] std::string   getTypeStr() const noexcept
    {
        std::bitset<32> bits(type);
        std::string result;
        for (int i = 24; i >= 0; i -= 8)
            result += static_cast<char>((bits >> i).to_ulong() & 0xFF);
        return result;
	}
};

class mp4aAtom : public atom
{

public:
    mp4aAtom(sPtrFile file) : atom(file) {}
};

class QTFF
{
    sPtrFile             fileStream;
    //sPtrQueueList<float> audio;

    template<typename T>
    T readBigEndian()
    {
        T value {};
        fileStream->read(reinterpret_cast<char*>(&value), sizeof(T));
        for (size_t i = 0; i < sizeof(T) / 2; ++i) 
            std::swap(reinterpret_cast<uint8_t*>(&value)[i], reinterpret_cast<uint8_t*>(&value)[sizeof(T) - 1 - i]);
        return value;
    }
public:
    QTFF(const std::string& filePath)
    {
        try
        {
            fileStream = std::make_shared<std::ifstream>();
            fs::path qtffPath(filePath);
            if (!fs::exists(qtffPath))
                throw std::runtime_error("Invalid file path.");
            fileStream->open(filePath, std::ios::binary);
            if (!fileStream->is_open())
                throw std::runtime_error("Failed to open file.");
            atom fileType(fileStream);
            if (!fileType.typeIs("ftyp"))
            {
                std::print("type is : {}", fileType.getTypeStr());
                throw std::runtime_error("Not a valid QTFF file.");
            }
            fileType.skip();
            //audio = std::make_shared<std::vector<audioQueue<float>>>(outputCfg);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::format("Failed to open QTFF file : {}.", e.what()));
        }
    }
   ~QTFF() { fileStream->close(); }

    atom searchAtom     (std::string_view code)
    {
        while (fileStream->good())
        {
            atom header(fileStream);
            if (header.typeIs(code))
                return header;
            else
                header.skip();
        }
        throw std::runtime_error("Atom not found.");
    }
    void searchAudioInfo()
    {
        auto moovAtom = searchAtom("moov");
        std::vector<atom> tracks;
        
        while (fileStream->tellg() < moovAtom.getEnd())
        {
            auto vtrakAtom = searchAtom("trak");
            vtrakAtom.skip();
            auto strakAtom = searchAtom("trak");
            auto mdiaAtom = searchAtom("mdia");
            auto minfAtom = searchAtom("minf");
            auto stblAtom = searchAtom("stbl");
            auto stsdAtom = searchAtom("stsd");
            
            /*
             * stsd atom structure
             * data fields            size    pos   status          diff
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
            std::uint32_t nbentries = readBigEndian<std::uint32_t>();

            for (auto i = 0; i < nbentries; i++)
            {
                auto mp4aAtom = searchAtom("mp4a");
                /*
                 * mp4a layout
                 * data fields            size    pos   status          diff
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
                 * -----------------------------> 28    pos after(1)    
                 * Reserved               2       28    ignore
                 * Sample Rate            4       30    target(3)       2 
                 */
                fileStream->seekg(16, std::ios::cur);//seek target(1)
                std::uint16_t channelCount = readBigEndian<std::uint16_t>();
                std::print("channelNum : {}\n", channelCount);

                //target(2)
                std::uint16_t sampleSize = readBigEndian<std::uint16_t>();
                std::print("sampleSize : {}\n", sampleSize);

                fileStream->seekg(2, std::ios::cur);//seek target(3)
                std::uint32_t sampleRate = readBigEndian<std::uint32_t>();
                std::print("sampleRate : {}\n", sampleRate);
                
            }
            //mp4aAtom.skip();
            break;
        }
    }
};

#endif