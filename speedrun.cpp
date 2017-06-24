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

/* Long names are bad if your're an adept of the Eighty Column religion. */
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
     * with care. More over, to tell the truth, dealing with ring buffer
     * idices really hurts.
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
 * for the particular search pattern. Shodan, this part of code is written
 * this way especially for you. Enjoy :).
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
        int j = patlen-1; // Don't make unsigned!
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

/*
 * Finds the first thing looking like an URL in the circular buffer 'buf'
 * in the [begin, end) range. If an URL was found, the function returns
 * 'true' and sets its results like in the following example:
 *
 *     https://www.youtube.com/watch?v=oHg5SJYRHA0
 *     ^       ^ domainBegin  ^     ^ urlEnd
 *     urlBegin               pathBegin
 *
 * If the function hasn't found anything like an URL, or found a thing
 * what, if continued beyond the 'end', may cause a longer match, it sets
 * all these four results to the same value which should be treated as the
 * search re-run point.
 */
template <UIndex N>
bool findUrl(RingArray<char, N> const& buf,
             UIndex  begin,
             UIndex  end,
             UIndex& urlBegin,
             UIndex& domainBegin,
             UIndex& pathBegin,
             UIndex& urlEnd) {
             // More arguments for the god of arguments.

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
        // The following line is a portability killer.
        // Please, don't run this program on IBM mainframes.
        return ('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ('0' <= ch && ch <= '9')
            || ch == '-' || ch == '.';
    };

    auto allowedInPath = [&allowedInDomainName](char ch) -> bool {
        return allowedInDomainName(ch)
            || ch == '_'
            || ch == '/'
            || ch == '+'
            || ch == ','; // I'm sure it a legal character, but who
    };                    // wants to put a comma in their URLs?

    urlBegin = httpBegin;
    idx = httpEnd-1;

    // Yes, you're right. The following is what you think. It's
    // a hand-written finite automaton. It's kind of ugly, but
    // is going to be faster than a regular expression.

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

// Sorts 'map' items by 'second' field, and prints the most frequent
// items as a text table.
void printTop(FrequencyMap const& map, UIndex maxNum){

    using Pointer = FrequencyMap::const_pointer;

    // Instead of copying pairs from the original map, simply fill
    // a vector with their addresses.
    std::vector<Pointer> pointers;
    pointers.reserve(map.size());
    for (auto& pair: map) {
        pointers.push_back(std::addressof(pair));
    }

    // Just sort the vector. I know, this is not not the best possible
    // approach from the O() point of view, but I have not enough spare
    // time for elaborated algorithms in a test assignment.
    std::sort(pointers.begin(), pointers.end(),
              [](Pointer const& a, Pointer const& b) {
                          return a->second > b->second; });

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
    if (!input.is_open()) {
        // Sorry, I'm not in mood to print errors nicely.
        throw std::ios_base::failure(inputFn);
    }

    // I thought that the buffer size of 8 kB would be large enough for
    // batch reading, yet small enough to fit the processor cache. However,
    // tests had shown that larger buffers operate faster.
    RingArray<char, 512*1024> buf;

    using OpState = std::tuple<bool, UIndex>;
    using Future = std::future<OpState>;

    // Starts a background file read operation to populate the [begin, end)
    // range of the circular buffer with fresh data.
    auto populate = [&](UIndex begin, UIndex end) -> Future {
        begin = buf.wrap(begin);
        end   = buf.wrap(end);

        return std::async(std::launch::async, [&input, &buf, begin, end]()
                                                               -> OpState {
            if (!input.good())
                return {false, begin};

            if (begin == end)
                throw std::logic_error("This should never happen.");

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
            // Why not just create the result string? I'm just trying
            // to avoid unneeded memory allocation.
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
            bool found = findUrl(buf, urlEnd, end,
                             urlBegin, domainBegin, pathBegin, urlEnd);
            if (!found)
                break;

            foundAny = true;
            ++numMatches;

            readString(urlDomain, domainBegin, pathBegin);
            if (pathBegin == urlEnd)
                urlPath = "/";
            else
                readString(urlPath, pathBegin, urlEnd);

            addEntry(urlDomains, urlDomain);
            addEntry(urlPaths, urlPath);
        }

        return {foundAny, urlEnd};
    };

    auto future = populate(0, buf.size()/2);
    future.wait();

    bool readAny;
    UIndex readEnd;
    std::tie(readAny, readEnd) = future.get();

    UIndex searchBegin = 0;
    UIndex searchEnd   = readEnd;

    while (readAny) {
        auto future = populate(searchEnd, searchBegin);

        bool matchAny;
        UIndex matchEnd;
        std::tie(matchAny, matchEnd) =
                                processMatches(searchBegin, searchEnd);

        future.wait();

        std::tie(readAny, readEnd) = future.get();
        if (!readAny)
            break;

        searchBegin = matchEnd;
        searchEnd = readEnd;
    }

    std::cout << "total urls " << numMatches   << ", "
              << "domains "    << urlDomains.size() << ", "
              << "paths "      << urlPaths.size() << std::endl
                                                  << std::endl;

    std::cout << "top domains" << std::endl;
    printTop(urlDomains, maxNum);
    std::cout << std::endl;

    std::cout << "top paths" << std::endl;
    printTop(urlPaths, maxNum);
}
