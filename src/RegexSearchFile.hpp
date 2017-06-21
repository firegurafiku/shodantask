#pragma once
#include <regex>
#include <functional>
#include <boost/noncopyable.hpp>
#include <boost/coroutine/coroutine.hpp>

class RegexMatch: public boost::noncopyable {
public:

    RegexMatch(std::function<std::string(int)> retriever)
        : mRetriever(retriever) {}

    std::string str(int groupIdx) const {
        return mRetriever(groupIdx);
    }

private:
    std::function<std::string(int)> mRetriever;
};

using RegexSearchCo = boost::coroutines::coroutine<RegexMatch const&>;

void regexSearchFileBuf(RegexSearchCo::push_type& yield,
                        std::string const& inputFn,
                        std::regex  const& rex,
                        size_t maxMatchLen,
                        size_t bufferSize);

void regexSearchFileMmap(RegexSearchCo::push_type& yield,
                         std::string const& inputFn,
                         std::regex  const& rex);
