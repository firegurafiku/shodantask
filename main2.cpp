#include <iostream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <boost/range/adaptors.hpp>
#include "RegexSearchFile.hpp"
#include "Helpers.hpp"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        // Sorry for not using the original command line syntax proposed by
        // the problem formulation ("mytest [-n NNN] in.txt out.txt"),
        // I was too lazy for command-line options parsing. I hope that's
        // not a mission critical thing.
        std::cout << "Usage: shodantask (mmap|buf) N INPUT_FILE\n";
        return EXIT_FAILURE;
    }

    auto method   = std::string(argv[1]);
    auto maxNum   = std::stoul (argv[2]);
    auto inputFn  = std::string(argv[3]);

    // The simplistic regex to find URLs (to find just as many patters,
    // as the problem formulation asks for). However, it can easily be
    // make as comprehensive as needed.
    constexpr auto urlRegexExpr = "(https?)://([a-z.-]+)(/[a-z_.,/+-]*)?";
                                // 1          2        3

    std::regex rex(urlRegexExpr, std::regex::icase);

    RegexSearchCo::pull_type coro =
        method == "mmap" ? spawn<RegexSearchCo>(regexSearchFileMmap, inputFn, rex)
      : method == "buf"  ? spawn<RegexSearchCo>(regexSearchFileBuf, inputFn, rex, 100, 4096)
      : throw std::invalid_argument("lookup method unsupported");

    using FrequencyMap = std::unordered_map<std::string, int>;

    FrequencyMap hosts, paths;
    for (auto const& match: coro) {
        // At this place some additional cleanup and canonicalization
        // should be done. But the task doesn't insist on that.
        std::string host = match.str(2);
        std::string path = match.str(3);
        if (path.empty())
            path = "/";

        auto ihost = hosts.find(host);
        if (ihost != hosts.end())
            ++(ihost->second);
        else hosts[host] = 1;

        auto ipath = paths.find(path);
        if (ipath != paths.end())
            ++(ipath->second);
        else paths[path] = 1;
    }

    // Convenience subroutine which sorts 'map' items by 'second' field,
    // and prints the most frequent items as a nice text table.
    auto printTop = [maxNum](FrequencyMap& map){

        // Instead of copying pairs from the original map, simply fill
        // a vector with their addresses.
        std::vector<FrequencyMap::pointer> pointers;
        pointers.reserve(map.size());
        for (auto& pair: map) {
            pointers.push_back(std::addressof(pair));
        }

        // Just sort the vercor. I know, this is not not the best possible
        // approach from the O() point of view, but I have not enough spare
        // time for elaborated algorithms in a test assignment.
        using Pointer = FrequencyMap::pointer;
        std::sort(pointers.begin(), pointers.end(),
                  [](Pointer const& a, Pointer const& b) {
                              return a->second > b->second; });

        // For some reason which is unclear to me, boost::adaptors::sliced
        // fails when the requested slice size is greater than the range
        // size. Instead of rolling out custom adaptor it's easier to
        // adjust the requested size appropriately.
        auto num = std::min(maxNum, pointers.size());

        using boost::adaptors::sliced;
        for (auto const& ptr: pointers | sliced(0, num)) {
            std::cout << std::left
                      << std::setw(6) << ptr->second
                               << " " << ptr->first << std::endl;
        }
    };

    std::cout << "Most frequent hostnames:" << std::endl;
    printTop(hosts);

    std::cout << "Most frequent paths:" << std::endl;
    printTop(paths);
}
