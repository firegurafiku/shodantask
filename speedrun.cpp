#include <cstring>
#include <iostream>
#include <ios>
#include <fstream>
#include <string>
#include <atomic>
#include <thread>
#include <future>
#include <cstdint>
#include <tuple>
#include <vector>
#include <algorithm>
#include <unordered_map>

/*
 * Integral type to store ring buffer index. This type is intentionally
 * made unsigned, since we're going to do shifts and bitwise operations
 * which are well-defined by the C++ standard only for unsigned types.
 * This makes some things less convenient, but there're no other option.
 */
using UIndex = std::size_t;

using FrequencyMap = std::unordered_map<std::string, unsigned>;

/*
 * Unfortunately, we cannot use 'boost::circular_buffer', so must roll out
 * our own implementation. The main point of the implementation is the
 * following: if we restrict buffer size to the power of two values, we can
 * get very cheap implementation of index wrap-around (single bitwise and).
 * If the size of buffer known at compile time, the compiler is able to
 * statically calculate the mask and inline it.
 */
template <typename T, UIndex N>
class RingArray {
public:

    UIndex   size()  const { return N; }
    T const* begin() const { return buf; }
    T      * begin()       { return buf; }
    T const* end()   const { return buf + N; }
    T      * end()         { return buf + N; }

    /*
     * Returns the canonical representation of 'idx'. Whatever 'idx' is,
     * return value fits the range [0, size()-1].
     */
    UIndex wrap(UIndex idx) const { return idx % N; }

    /*
     * Index the underlying array with a wrapped around index value. Since
     * every possible 'idx' value is correct, these functions must be used
     * with care.
     */
    T const& operator [] (UIndex idx) const { return buf[idx % N]; }
    T      & operator [] (UIndex idx)       { return buf[idx % N]; }

private:

    T buf[N];
};

/*
 * An ad-hoc implementation for a function which looks for literal sting
 * "http" in a ring buffer of chars. It uses Wikipedia's implementation of
 * Boyer-Moore string search algorithm with search tables precalculated
 * for the particular search pattern.
 */
template <UIndex N>
bool findHttp(RingArray<char, N> const& buf,
              UIndex begin, UIndex end, UIndex& matchBegin, UIndex& matchEnd) {

    constexpr UIndex patlen = 4;
    constexpr const char* pat = "http";

    static UIndex delta1[] = {
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 };

    static UIndex delta2[] = {7, 6, 5, 1};

    begin = buf.wrap(begin);
    end   = buf.wrap(end);

    UIndex stringlen = buf.wrap(end - begin);
    if (stringlen == 0)
        stringlen = buf.size();

    if (stringlen < 4) {
        matchBegin = begin;
        matchBegin = end;
        return false;
    }

    UIndex i = patlen-1 +1;
    while (i < stringlen+1) {
        int j = patlen-1;
        while (j >= 0 && (buf[begin+i-1] == pat[j])) {
            --i;
            --j;
        }
        if (j < 0) {
            matchBegin = buf.wrap(begin+i);
            matchEnd   = buf.wrap(matchBegin + 4);
            return true;
        }

        i += std::max(delta1[buf[begin+i-1]], delta2[j]);
    }

    matchBegin = end - 3;
    matchEnd   = end - 3;
    return false;
}

template <UIndex N>
bool findUrl(RingArray<char, N> const& buf,
             UIndex begin, UIndex end, UIndex& urlBegin, UIndex& domainBegin, UIndex& pathBegin, UIndex& urlEnd) {

    UIndex httpBegin, httpEnd;
    bool foundHttp = findHttp(buf, begin, end, httpBegin, httpEnd);

    if (!foundHttp) {
        urlBegin = domainBegin = pathBegin == httpEnd;
        return false;
    }

    UIndex idx;
    char ch;

    auto advance = [&buf, &idx, &ch, end]{
        idx = buf.wrap(idx+1);
        ch = buf[idx];
    };

    auto allowedInDomainName = [](char ch) -> bool {
        return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '-' || ch == '.';
    };

    auto allowedInPath = [&allowedInDomainName](char ch) -> bool {
        return allowedInDomainName(ch) || ch == '/' || ch == ',';
    };

    urlBegin = httpBegin;

    idx = httpEnd-1;

got_http:
    advance();
    if (idx == end)
        goto fail_premature;

    if (ch == ':')
        goto got_http_colon;
    if (ch == 's')
        goto got_https;

    goto fail;

got_http_colon:
    advance();
    if (idx == end)
        goto fail_premature;

    if (ch == '/')
        goto got_first_slash;

    goto fail;

got_https:
    advance();
    if (idx == end)
        goto fail_premature;

    if (ch == ':')
        goto got_https_colon;

    goto fail;

got_https_colon:
    advance();
    if (idx == end)
        goto fail_premature;

    if (ch == '/')
        goto got_first_slash;

    goto fail;

got_first_slash:
    advance();
    if (idx == end)
        goto fail_premature;

    if (ch == '/')
        goto got_second_slash;

    goto fail;

got_second_slash:
    advance();
    if (idx == end)
        goto fail_premature;

    domainBegin = idx;
    if (allowedInDomainName(ch))
        goto got_domain_char;

    goto fail;

got_domain_char:
    advance();
    if (idx == end)
        goto fail_premature;

    if (ch == '/') {
        pathBegin = idx;
        goto got_path_char;
    }

    if (allowedInDomainName(ch))
        goto got_domain_char;

    goto out_domain;

got_path_char:
    advance();
    if (idx == end)
        goto fail_premature;

    if (allowedInPath(ch))
        goto got_path_char;

    urlEnd = idx;
    goto out_path;

out_domain:
    urlEnd = idx;
    pathBegin = idx;
    return true;

out_path:
    urlEnd = idx;
    return true;

fail_premature:
    urlBegin = domainBegin = pathBegin = urlEnd = httpBegin;
    return false;

fail:
    urlBegin = domainBegin = pathBegin = httpEnd;
    return false;
}

// Convenience subroutine which sorts 'map' items by 'second' field,
// and prints the most frequent items as a nice text table.
void printTop(FrequencyMap const& map, UIndex maxNum){

    using Pointer = FrequencyMap::const_pointer;

    // Instead of copying pairs from the original map, simply fill
    // a vector with their addresses.
    std::vector<Pointer> pointers;
    pointers.reserve(map.size());
    for (auto& pair: map) {
        pointers.push_back(std::addressof(pair));
    }

    // Just sort the vercor. I know, this is not not the best possible
    // approach from the O() point of view, but I have not enough spare
    // time for elaborated algorithms in a test assignment.
    std::sort(pointers.begin(), pointers.end(),
              [](Pointer const& a, Pointer const& b) {
                          return a->second > b->second; });

    // For some reason which is unclear to me, boost::adaptors::sliced
    // fails when the requested slice size is greater than the range
    // size. Instead of rolling out custom adaptor it's easier to
    // adjust the requested size appropriately.
    auto num = std::min(maxNum, pointers.size());

    UIndex i = 0;
    for (auto const& ptr: pointers) {
        if (i++ >= maxNum)
            break;

        std::cout << ptr->second << ' ' << ptr->first << std::endl;
    }
}

int main(int argc, char *argv[]) {
    std::string inputFn;
    std::string outputFn;
    unsigned maxNum;

    /*
     * I don't really understand why the task formulation insists on the
     * optional command line switch "-n". It adds routine to the code with
     * no benefit (comparing to required argument).
     */
    if (argc == 3) {
        maxNum   = 10;
        inputFn  = argv[1];
        outputFn = argv[2];
    } else if (argc == 5 && std::string(argv[1]) == "-n") {
        maxNum   = std::stoul(argv[2]);
        inputFn  = argv[3];
        outputFn = argv[4];
    } else {
        std::cerr << "Usage: speedrun [-n N] INPUT OUTPUT\n";
        return EXIT_FAILURE;
    }

    std::ifstream input(inputFn);
    if (!input.is_open())
        throw std::exception();

    // The buffer size of 2**13 bytes (== 8 kB) should be large enough for
    // batch reading, yet small enough to fit the processor cache.
    RingArray<char, 8*1024> buf;

    using OpState = std::tuple<bool, UIndex>;
    using Future = std::future<OpState>;

    auto populate = [&](UIndex begin, UIndex end) -> Future {
        begin = buf.wrap(begin);
        end   = buf.wrap(end);

        return std::async(std::launch::async, [&input, &buf, begin, end]() -> OpState {
            if (!input.good())
                return {false, begin};

            if (begin == end)
                throw std::exception();

            std::streamsize read = 0;
            if (begin < end) {
                input.read(&buf[begin], end - begin);
                read += input.gcount();
            }
            else {
                input.read(&buf[begin], buf.size() - begin);
                read += input.gcount();

                input.read(&buf[0], end);
                read += input.gcount();
            }

            if (read == 0)
                return {false, begin};

            return {true, buf.wrap(begin + read)};
        });
    };


    FrequencyMap urlDomains, urlPaths;
    unsigned numMatches = 0;

    auto processMatches = [&](UIndex begin, UIndex end) -> OpState {
        begin = buf.wrap(begin);
        end   = buf.wrap(end);

        std::string urlDomain;
        std::string urlPath;

        auto readString = [&](std::string& dest, UIndex begin, UIndex end) {
            dest.clear();
            for (UIndex i = begin; i != end; i = buf.wrap(i+1))
                dest.push_back(buf[i]);
        };

        auto addEntry = [](FrequencyMap& map, std::string const& key) {
            auto iter = map.find(key);
            if (iter != map.end())
                ++(iter->second);
            else map[key] = 1;
        };

        UIndex urlBegin, domainBegin, pathBegin, urlEnd = begin;
        bool foundAny = false;
        while (1) {
            bool found = findUrl(buf, urlEnd, end, urlBegin, domainBegin, pathBegin, urlEnd);
            if (!found)
                break;

            // Check.
            if (buf[urlBegin+0] != 'h'
             || buf[urlBegin+1] != 't'
             || buf[urlBegin+2] != 't'
             || buf[urlBegin+3] != 'p')
                std::logic_error("Found something bad");

            foundAny = true;
            ++numMatches;

            readString(urlDomain, domainBegin, pathBegin);
            if (pathBegin == urlEnd)
                urlPath = "/";
            else
                readString(urlPath, pathBegin, urlEnd);

            addEntry(urlDomains, urlDomain);
            addEntry(urlPaths, urlPath);

            //std::cout << "FOUND: " << ' ' << urlDomain << ' ' << urlPath << std::endl;
        }

        return {foundAny, urlEnd};
    };

    auto future = populate(0, buf.size()/2);
    future.wait();

    bool readAny;
    UIndex readEnd;
    std::tie(readAny, readEnd) = future.get();
    if (!readAny)
        return 0;

    UIndex searchBegin = 0;
    UIndex searchEnd   = readEnd;

    while (1) {
        auto future = populate(searchEnd, searchBegin);

        bool matchAny;
        UIndex matchEnd;
        std::tie(matchAny, matchEnd) =
                                processMatches(searchBegin, searchEnd);

        //std::cout << "HERE\n";
        future.wait();

        bool readAny;
        UIndex readEnd;
        std::tie(readAny, readEnd) = future.get();
        if (!readAny)
            break;

        // Buffer too small?
        if (matchEnd == searchBegin)
            ;

        searchBegin = matchEnd;
        searchEnd = readEnd;
    }

    std::cout << "total urls " << numMatches   << ", "
              << "domains "    << urlDomains.size() << ", "
              << "paths "      << urlPaths.size() << std::endl;
    std::cout << "Most frequent hostnames:" << std::endl;
    printTop(urlDomains, maxNum);

    std::cout << "Most frequent paths:" << std::endl;
    printTop(urlPaths, maxNum);
}
