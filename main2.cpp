#include <boost/numeric/conversion/cast.hpp>
#include "BufferedUrlExtractor.hpp"
#include "MMappedUrlExtractor.hpp"

int main(int argc, char *argv[]) {
    if (argc != 5)
        return 1;

    auto method   = std::string(argv[1]);
    auto maxNum   = std::stoul(argv[2]);
    auto inputFn  = std::string(argv[3]);
    auto outputFn = std::string(argv[4]);

    decltype(&extractUrlsUsingCircularBuffer) extractorImpl =
                        method == "mmap" ? nullptr
                      : method == "buf"  ? extractUrlsUsingCircularBuffer
                      : nullptr;

    if (!extractorImpl)
        return 1;

    namespace coro = boost::coroutines2;
    UrlExtractor::pull_type urlSeq(
            coro::fixedsize_stack(),
            [&](auto& yield) { return extractorImpl(inputFn, yield); });

    for (auto s: urlSeq) {
        std::cout << s << std::endl;
    }
}
