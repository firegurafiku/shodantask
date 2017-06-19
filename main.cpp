#include <iostream>
#include <regex>
#include <boost/range/iterator_range.hpp>
#include "MemoryMappedFile.hpp"

int main(int argc, char *argv[]) {
    int n = 10;
    std::string inputFn  = "input.txt";
    std::string outputFn = "output.txt";
    
    constexpr auto urlRegexExpr = "https?://[a-z.]+(/[a-z.,/]*)?";
    std::regex urlRegex(urlRegexExpr, std::regex::icase);

    MemoryMappedFile file(inputFn);
    file.open();

    auto matchesEnd   = std::cregex_iterator();
    auto matchesBegin = std::cregex_iterator(
                                    file.begin(), file.end(), urlRegex);

    for (auto match: boost::make_iterator_range(matchesBegin, matchesEnd)) {
        std::cout << match.str() << std::endl;
    }
}


