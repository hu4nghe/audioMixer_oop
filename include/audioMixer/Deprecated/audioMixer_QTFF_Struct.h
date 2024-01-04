#ifndef AUDIOMIXER_QTFF_H
#define AUDIOMIXER_QTFF_H

#include <fstream>
#include <print>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <bitset>

//criteria function concept
template <typename F, typename Arg>
concept Criteria = requires(F func, Arg arg){ { func(arg) } -> std::same_as<bool>; };

//std::shared_ptr alias
template <typename T>
using sPtr = std::shared_ptr<T>;

//convert 32bit int to fourCC code
static std::string getStr(std::uint32_t code)
{
    std::bitset<32> bits(code);
    std::string result;
    for (int i = 24; i >= 0; i -= 8)
        result += static_cast<char>((bits >> i).to_ulong() & 0xFF);
    return result;
}

class fileReader : public std::ifstream
{
public:
    fileReader& readBigEndian(char* buffer, std::streamsize count)
    {
        if (!this->good())
            throw std::ios_base::failure("Error reading from file.");
        try
        {
            auto num = this->rdbuf()->sgetn(buffer, count);

            for (auto i = 0; i < count / 2; ++i)
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
            std::swap(reinterpret_cast<std::uint8_t*>(&value)[i                ], 
                      reinterpret_cast<std::uint8_t*>(&value)[sizeof(T) - 1 - i]);
    }
};

class atom : public std::enable_shared_from_this<atom> 
{
public:
    std::uint32_t    atomSize;
    std::uint32_t    atomType;
    std::  size_t    posBegin;
    sPtr<fileReader> file;
    std::map<std::string, sPtr<atom>> children;

    atom() = default;
    atom(sPtr<fileReader> fileStream)
    {
        try
        {
            file = fileStream;
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

    void restart(){ file->seekg(posBegin); }
    void skip   ()
    { 
        restart();
        file->seekg(atomSize, std::ios::cur);
    }
    bool typeIs (std::string_view fourCC) const { return fourCC == getStr(atomType); }

    void addChild(const sPtr<atom>& child){ children[child->data] = child; }

    [[nodiscard]] std::uint32_t type() const noexcept { return atomType; }
    [[nodiscard]] std::uint32_t size() const noexcept { return atomSize; }
    [[nodiscard]] std::uint32_t head() const noexcept { return posBegin; }
    [[nodiscard]] std::uint32_t end () const noexcept { return posBegin + atomSize; }
};

class Tree 
{
public:
    sPtr<atom> root;
    Tree(atom&& rootNode) : root(std::make_shared<atom>(std::move(rootNode))) {}

    template <typename Criteria>
    auto searchNodes(const sPtr<atom>& currentNode, Criteria auto& criteriaFunction)
    {
        std::vector<atom> satisfied;
        // Check if the current node satisfies the criteria
        if (criteriaFunction(currentNode))
            satisfied.push_back(currentNode);

        // Recursively search the children
        for (const auto& [key, child] : currentNode->children) 
        {
            searchNodes(child, criteriaFunction);
        }
    }
};

#endif //AUDIOMIXER_QTFF_H