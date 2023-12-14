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
            std::print("current atom head : {}\n", posBegin);
            std::print("current atom end  : {}\n", posBegin + size);
        }
        catch (const std::exception& e)
        {
            std::print(stderr, "Error : {}.\n", e.what());
            throw std::runtime_error("Failed to read file.");
        }
    }
   ~atom() = default;

    void skip()
    {
        file->seekg(posBegin);
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
    sPtrFile fileStream;

    std::uint32_t readBigEndian()
    {
        std::uint32_t rawData = 0;
        fileStream->read(reinterpret_cast<char*>(&rawData), sizeof(rawData));
        rawData = ((rawData & 0xFF) << 24) | (((rawData >> 8) & 0xFF) << 16) | (((rawData >> 16) & 0xFF) << 8) | ((rawData >> 24) & 0xFF);
        return rawData;
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
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::format("Failed to open QTFF file : {}.", e.what()));
        }
    }
   ~QTFF() { fileStream->close(); }
    [[nodiscard]] std::size_t getPos() { return fileStream->tellg(); }

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
        size_t posBegin = fileStream->tellg();
        size_t moovEndPos = posBegin + moovAtom.getSize();
        while (fileStream->tellg() < moovEndPos)
        {
            auto vtrakAtom = searchAtom("trak");
            vtrakAtom.skip();
            auto strakAtom = searchAtom("trak");
            auto mdiaAtom = searchAtom("mdia");
            auto minfAtom = searchAtom("minf");
            auto stblAtom = searchAtom("stbl");
            auto stsdAtom = searchAtom("stsd");
            
            fileStream->seekg(4, std::ios::cur);
            std::uint32_t nbentries = readBigEndian();
            std::print("number of entries : {}\n", nbentries);
            auto mp4aAtom = searchAtom("mp4a");
            fileStream->seekg(40, std::ios::cur);
            std::uint32_t channelNum = readBigEndian();
            std::print("channelNum : {}\n",channelNum);

            //mp4aAtom.skip();
            break;
        }
    }
};

#endif