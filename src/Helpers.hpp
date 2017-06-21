#pragma once
#include <tuple>
#include <boost/range/iterator_range.hpp>
#include <boost/coroutine/coroutine.hpp>

/*
 * Copies items from [first, last) to 'dest', but no more than 'count'.
 * I hoped the standard library already has such an algorithm, but it does
 * not. So I had to roll out my own implementation.
 *
 * In this function iterator arguments are passed by value, not by constant
 * reference. I did this because 'std::copy' does the same thing.
 */
template <class InputIt, class OutputIt>
std::tuple<InputIt, OutputIt> copyUpTo(InputIt first, InputIt last,
                                         size_t count, OutputIt dest){
    InputIt it = first;
    for ( ; it != last && count != 0; --count, ++it) {
        *dest = *it;
        ++dest;
    }
    return {it, dest};
}

/*
 * Creates a pair of 'std::regex_iterator' and wraps them into a Boost
 * range. This function makes searching every match in a string as easy as
 * this simple for loop:
 *
 *     for (auto const& match: regex_search_all(first, last, rex))
 *         ; // Do something with 'match'.
 *
 * I dunno why where is no such function in the standard library.
 */
template <typename BidirIt>
auto regexSearchAll(BidirIt first, BidirIt last, std::regex const& rex)
                    -> boost::iterator_range<std::regex_iterator<BidirIt>> {

    using RegexIterator = std::regex_iterator<BidirIt>;
    RegexIterator matchesBegin (first, last, rex);
    RegexIterator matchesEnd;

    return {matchesBegin, matchesEnd};
}

/*
 * Convenience function to spawn coroutines. The first argument of 'func'
 * must be of type 'Coro::push_type&', the rest of arguments are forwarded
 * from 'args' pack. Use the whole thing as the following:
 *
 *     using Coro = boost::coroutines::coroutine<std::string>;
 *     void func(Coro::push_type& yield, int x, float y, std::sting s);
 *
 *     for (std::string result: spawn<Coro>(f, 42, 2.7, "KILLMEPLZ")
 *         ; // do something useful with 'result'.
 */
template <typename Coro, typename Anything, typename... Args>
auto spawn(Anything func, Args&&... args)
    -> typename Coro::pull_type {

    auto wrapped_func = [&](typename Coro::push_type& yield) {
        return func(yield, std::forward<Args>(args)...);
    };

    return typename Coro::pull_type {wrapped_func};
}
