#pragma once
#include <algorithm>
#include <numeric>
#include <iostream>
#include <ios>
#include <fstream>
#include <string>
#include <regex>
#include <boost/coroutine2/coroutine.hpp>
#include <boost/range/iterator_range.hpp>
#include "MemoryMappedFile.hpp"

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

template <class InputIt, class OutputIt>
std::tuple<InputIt, OutputIt>
copy_up_to(InputIt const& first, InputIt const& last, size_t count, OutputIt dest){
    InputIt it = first;
    for ( ; it != last && count != 0; --count, ++it) {
        *dest = *it;
        ++dest;
    }
    return {it, dest};
}

template <typename BidirIt>
boost::iterator_range<std::regex_iterator<BidirIt>>
regex_search_all(BidirIt const& first, BidirIt const& last, std::regex const& rex) {
    using RegexIterator = std::regex_iterator<BidirIt>;
    RegexIterator matchesBegin(first, last, rex);
    RegexIterator matchesEnd {};
    return { matchesBegin, matchesEnd };
}

using UrlExtractor = boost::coroutines2::coroutine<std::string>;
void extractUrlsUsingCircularBuffer(std::string const& inputFn,
                                    UrlExtractor::push_type & yield) {

    constexpr auto urlRegexExpr = "https?://[a-z.]+(/[a-z.,/]*)?";
    std::regex urlRegex(urlRegexExpr, std::regex::icase);

    const size_t reasonableUrlLength = 40;
    boost::circular_buffer<char> buf(100);

    std::ifstream input(inputFn);
    input.unsetf(std::ios_base::skipws);
    auto fileIter = std::istream_iterator<char>(input);
    auto fileEnd  = std::istream_iterator<char>();

    auto populate = [&fileIter, &fileEnd, &buf](size_t n) {
        std::tie(fileIter, std::ignore) =
                copy_up_to(fileIter, fileEnd, n, std::back_inserter(buf));
    };

    populate(buf.capacity());
    while (1) {
        auto lastMatchEnd = buf.end();
        bool hadMatches = false;
        for (auto const& match: regex_search_all(buf.begin(), buf.end(), urlRegex)) {
            std::clog << "match\n";
            yield(match.str());
            lastMatchEnd = match[0].second;
            hadMatches = true;
        }

        if (fileIter == fileEnd)
            break;

        size_t margin = hadMatches
                ? buf.end() - lastMatchEnd
                : reasonableUrlLength;

        populate(buf.capacity() - margin);
    }
}
