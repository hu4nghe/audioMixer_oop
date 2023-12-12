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

std::string getStr(std::uint32_t code)
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
	std::uint32_t   size;
	std::uint32_t   type;
    std::streampos  pos;
public:
    atom(std::ifstream& file)
    {
        try
        {
            pos = file.tellg();
            file.read(reinterpret_cast<char*>(&size), sizeof(std::uint32_t));
            file.read(reinterpret_cast<char*>(&type), sizeof(std::uint32_t));
            type = ((type & 0xFF) << 24) | (((type >> 8) & 0xFF) << 16) | (((type >> 16) & 0xFF) << 8) | ((type >> 24) & 0xFF);
            size = ((size & 0xFF) << 24) | (((size >> 8) & 0xFF) << 16) | (((size >> 16) & 0xFF) << 8) | ((size >> 24) & 0xFF);
            std::print("type : {}\nsize : {}\n", this->getTypeStr(), size);
        }
        catch (const std::exception& e)
        {
            std::print(stderr, "Error : {}.\n", e.what());
            throw std::runtime_error("Failed to read file.");
        }
        
    }
   ~atom() = default;

    void gotoEnd(std::ifstream& file) const // declared const, but do modify the position of file pointer.
    {
        file.seekg(pos);
        int i = file.tellg();
        std::print("current atom head : {}\n", i);
        file.seekg(size, std::ios::cur); 
        i = file.tellg();
        std::print("current atom end  : {}\n", i);
    }

    

    [[nodiscard]] std::uint32_t getType   () const noexcept{ return type; }
    [[nodiscard]] std::uint32_t getSize   () const noexcept{ return size; }
    [[nodiscard]] std::string   getTypeStr() const noexcept
    {
        std::bitset<32> bits(type);
        std::string result;
        for (int i = 24; i >= 0; i -= 8)
            result += static_cast<char>((bits >> i).to_ulong() & 0xFF);
        return result;
	}

    bool typeIs(std::string_view fourCC) const
    {
        if (fourCC.size() != 4)
            throw std::invalid_argument("Input FourCC must have exactly four characters.");
        return fourCC == this->getTypeStr();
    }
};

class QTFF
{
    std::ifstream fileStream;

    std::uint32_t readBigEndian()
    {
        std::uint32_t target = 0;
        fileStream.read(reinterpret_cast<char*>(&target), sizeof(target));
        target = ((target & 0xFF) << 24) | (((target >> 8) & 0xFF) << 16) | (((target >> 16) & 0xFF) << 8) | ((target >> 24) & 0xFF);
        return target;
    }
public:
    QTFF(const std::string& filePath)
    {
        try
        {
            fs::path qtffPath(filePath);
            if (!fs::exists(qtffPath))
                throw std::runtime_error("Invalid file path.");
            fileStream.open(filePath, std::ios::binary);
            if (!fileStream.is_open())
                throw std::runtime_error("Failed to open file.");
            atom fileType(fileStream);
            if (!fileType.typeIs("ftyp"))
            {
                std::print("type is : {}", fileType.getTypeStr());
                throw std::runtime_error("Not a valid QTFF file.");
            }
            fileType.gotoEnd(fileStream);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::format("Failed to open QTFF file : {}.", e.what()));
        }
    }
   ~QTFF() { fileStream.close(); }
    [[nodiscard]] std::size_t getPos() { return fileStream.tellg(); }

    atom searchAtom(std::string_view code)
    {
        while (fileStream.good())
        {
            atom header(fileStream);
            if (!header.typeIs(code))
                header.gotoEnd(fileStream);
            else
                return header;
        }
        throw std::runtime_error("Atom not found.");
    }

    void searchAudioInfo()
    {
        auto moovAtom = searchAtom("moov");
        size_t pos = fileStream.tellg();
        size_t moovEndPos = pos + moovAtom.getSize();
        while (fileStream.tellg() < moovEndPos)
        {
            auto trakAtom = searchAtom("trak");
            auto mdiaAtom = searchAtom("mdia");
            auto hdlrAtom = searchAtom("hdlr");

            auto hdlrSubType = readBigEndian();
            fileStream.seekg(8, std::ios::cur);
            
            if (getStr(hdlrSubType) == "soun")
                std::print("sound track found.\n");
        }
    }
};

#endif