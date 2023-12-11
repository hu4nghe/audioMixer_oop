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
class atom
{
protected:
	std::uint32_t size;
	std::uint32_t type;
public:
	         atom() = default;
    virtual ~atom() = default;

    virtual void read(std::ifstream& file)
    {
        file.read(reinterpret_cast<char*>(&size), sizeof(std::uint32_t));
        file.read(reinterpret_cast<char*>(&type), sizeof(std::uint32_t));
    }
    std::uint32_t getType() const noexcept{ return type; }
    std::uint32_t getSize() const noexcept{ return size; }
	std::string getTypeStr() const noexcept
	{
        std::bitset<32> bits(type);
        std::string result;
        for (int i = 24; i >= 0; i -= 8)
            result += static_cast<char>((bits >> i).to_ulong() & 0xFF);
        return result;
	}
};

class ftyp : public atom
{
                std::uint32_t  majorBrand;
                std::uint32_t  minorVersion;
    std::vector<std::uint32_t> compatibleBrands;
public: 
    ftyp() = default;
    void read(std::ifstream& file) override
    {
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        file.read(reinterpret_cast<char*>(&type), sizeof(type));
        file.read(reinterpret_cast<char*>(&majorBrand), sizeof(majorBrand));
        file.read(reinterpret_cast<char*>(&minorVersion), sizeof(minorVersion));
    }
};

class QTFF
{
    std::ifstream fileStream;
    bool isContainerAtom(std::uint32_t atomType) 
    {
        return (atomType == 0x6D6F6F76) || (atomType == 0x7472616B);
    }
public:

    bool openFile(const std::string& filePath) 
    {
        try 
        {
            fs::path qtffPath(filePath);
            if (!fs::exists(qtffPath))
                throw std::runtime_error("Invalid file path.");
            
            fileStream.open(filePath, std::ios::binary);

            if (!fileStream.is_open())
                throw std::runtime_error("Failed to open file.");

            char fileType[4];
            
            uint32_t version ;

            fileStream.read(fileType, 4);
            fileStream.read(reinterpret_cast<char*>(&version), sizeof(version));

            // Verify the file type and version
            if (std::string(fileType) != "ftyp")
                throw std::runtime_error("Not a valid QTFF file.");
            
            return true;
        }
        catch (const std::exception& e) 
        {
            std::print(stderr, "Error : {}.\n", e.what());
            return false;
        }
    }
    void parseAtom()
    {
        atom header;
        header.read(fileStream);

        if (isContainerAtom(header.getType())) 
        {
            std::streampos startPosition = fileStream.tellg();
            std::streampos endPosition   = startPosition + header.getSize() - sizeof(atom);

            // Recursively parse the contents of the container atom
            while (fileStream.tellg() < endPosition) 
                parseAtom();
            fileStream.seekg(endPosition, std::ios::beg);
        }
        else 
        {
            // Handle other atom types as needed
            // You can add specific parsing logic for known atom types
            std::cout << "Parsing logic for non-container atom..." << std::endl;
        }
    }
};

#endif