#include "RegexSearchFile.hpp"

#include <fstream>
#include "Helpers.hpp"
#include "MemoryMappedFile.hpp"

/*
 * Boost.CircularBuffer is too peaky about its iterators in debug mode and
 * triggers an *assert* even on innocent-looking code like this:
 *
 *     boost::circular_buffer<char> cb;
 *     cb.begin() == circular_buffer<char>::const_iterator();
 *
 * This was presented as a debug helper, hah.
 */
#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

void regexSearchFileMmap(RegexSearchCo::push_type& yield,
                         std::string const& inputFn,
                         std::regex  const& rex) {

    MemoryMappedFile file(inputFn);
    file.open();

    for (auto const& m: regexSearchAll(file.begin(), file.end(), rex)) {
        RegexMatch match ([&m](int gidx) {return m[gidx].str(); });
        yield(match);
    }
}

void regexSearchFileBuf(RegexSearchCo::push_type& yield,
                        std::string const& inputFn,
                        std::regex  const& rex,
                        size_t maxMatchLen,
                        size_t bufferSize) {

    std::ifstream input;
    input.open(inputFn);
    input.unsetf(std::ios_base::skipws);

    // Unfortunately, one cannot simply call input.exceptions() and live
    // happily (I know, I tried): exceptions and istream iterators are
    // notoriously hard to combine.
    if (!input.is_open())
        throw std::ios_base::failure(inputFn);

    auto fileIter = std::istream_iterator<char>(input);
    auto fileEnd  = std::istream_iterator<char>();

    boost::circular_buffer<char> buf(bufferSize);

    // A shorthand for convenience.
    auto populate = [&fileIter, &fileEnd, &buf](size_t n) {
        for (size_t i=0; i<n; ++i)
            buf.push_back(*fileIter++);
    };

    populate(buf.capacity());
    while (1) {
        auto lastMatchEnd = buf.end();
        bool hadMatches = false;
        for (auto const& m: regexSearchAll(buf.begin(), buf.end(), rex)) {
            RegexMatch match ([&m](int gidx) {return m[gidx].str(); });
            lastMatchEnd = m[0].second;
            hadMatches = true;

            // Let the caller examine the match.
            yield(match);
        }

        if (fileIter == fileEnd)
            break;

        size_t margin = hadMatches
                ? size_t(buf.end() - lastMatchEnd)
                : maxMatchLen;

        populate(buf.capacity() - margin);
    }

    if (!input.eof())
        throw std::ios_base::failure("cannot read the whole file");
}
