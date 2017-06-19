#include "MemoryMappedFile.hpp"
#include <system_error>

extern "C" {
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <string.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <sys/mman.h>
}

MemoryMappedFile::MemoryMappedFile()
    : mRegionAddr(nullptr)
    , mRegionLength(0)
    {}

MemoryMappedFile::MemoryMappedFile(char const* filename)
    : MemoryMappedFile() {

    mFilename = filename;
    open();
}

MemoryMappedFile::MemoryMappedFile(std::string const& filename)
    : MemoryMappedFile() {

    mFilename = filename;
    open();
}

MemoryMappedFile::~MemoryMappedFile() {
    try {
        close();
    } catch (...) {
        // Life is hard.
    }
}

void MemoryMappedFile::open() {
    int fd = -1;
    void *addr = MAP_FAILED;
    off_t length = 0;

    fd = ::open(mFilename.c_str(), O_RDONLY, 0);
    if (fd == -1)
        goto e_failure;

    length = ::lseek(fd, 0, SEEK_END);
    if (length == -1)
        goto e_failure;

    addr = ::mmap(0, length, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        goto e_failure;

    if (::close(fd) == -1)
        goto e_failure;

    //
    mRegionAddr = addr;
    mRegionLength = length;
    return;

e_failure:
    int errorNo = errno;
    if (fd != -1) {
        // Sorry, no more error handling for today.
        ::close(fd);
    }

    throw std::system_error(errorNo, std::system_category(), mFilename);
}


void MemoryMappedFile::close() {
    if (mRegionAddr == nullptr)
        return;

    if (::munmap(mRegionAddr, mRegionLength) == -1)
        goto e_failure;

    mRegionAddr = nullptr;
    mRegionLength = 0;
    return;

e_failure:
    throw std::system_error(errno, std::system_category(), mFilename);
}
