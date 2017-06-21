#pragma once
#include <string>
#include <boost/noncopyable.hpp>

class MemoryMappedFile: public boost::noncopyable {
public:

    using const_iterator = char*;

    MemoryMappedFile();
    MemoryMappedFile(char const* filename);
    MemoryMappedFile(std::string const& filename);

    ~MemoryMappedFile();

    void open();
    void close(bool noThrow = false);

    bool isOpen() const { return mRegionAddr != nullptr; }

    const_iterator begin() const {
        return static_cast<char*>(mRegionAddr);
    }

    const_iterator end() const {
        return begin() + mRegionLength;
    }

private:

    std::string mFilename;
    void  *mRegionAddr;
    size_t mRegionLength;
};
