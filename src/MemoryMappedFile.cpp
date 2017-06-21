#include "MemoryMappedFile.hpp"
#include <system_error>

extern "C" {
    #include <errno.h>
    #include <fcntl.h>
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

    // Everybody knows destructors must not throw (someone even knows why).
    // So, in this code I used the simple 'catch (...)' approach at first,
    // but then I read the following sentence in the Boost.Coroutine2 docs:
    //
    //   "Code executed by coroutine-function must not prevent the
    //    propagation of the 'detail::forced_unwind' exception. Absorbing
    //    that exception will cause stack unwinding to fail."
    //
    // Since I can never be sure about some library not trying to abuse
    // exceptions for execution flow control, I solemnly swear not to
    // use 'catch (...)' for the rest of my life.
    close(true);
}

void MemoryMappedFile::open() {
    int fd = -1;
    void *addr = MAP_FAILED;
    off_t length = 0;

    fd = ::open(mFilename.c_str(), O_RDONLY, 0);
    if (fd == -1)
        goto e_failure; // I've never been a member of Dijkstra's fan-club.

    length = ::lseek(fd, 0, SEEK_END);
    if (length == -1)
        goto e_failure;

    if (length > SIZE_MAX)
        goto e_failure;

    addr = ::mmap(0, (size_t)length, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        goto e_failure;

    if (::close(fd) == -1)
        goto e_failure;

    // Delay these assignments until the moment when nothing bad
    // can happen. Exception safety, kinda.
    mRegionAddr = addr;
    mRegionLength = (size_t)length;
    return;

e_failure:
    int errorNo = errno;
    if (fd != -1) {
        // Sorry, no more error handling for today. To be honest,
        // I just don't know what the program should do if the following
        // call fails.
        ::close(fd);
    }

    throw std::system_error(errorNo, std::system_category(), mFilename);
}

void MemoryMappedFile::close(bool noThrow) {
    if (mRegionAddr == nullptr)
        return;

    if (::munmap(mRegionAddr, mRegionLength) == -1)
        goto e_failure;

    mRegionAddr = nullptr;
    mRegionLength = 0;
    return;

e_failure:
    if (noThrow)
        return;

    throw std::system_error(errno, std::system_category(), mFilename);
}
