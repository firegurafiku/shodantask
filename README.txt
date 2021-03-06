The test assignment for cpp-school.unigine.com
==============================================

Hi all, guys.

Surely, you know that the problem you've proposed as the school's entry
test can easily be solved with a regular expression. Good news, C++11 had
introduced regex facilities into the standard library years ago. Bad news,
'std::regex_search' wants a pair of bidirectional iterators in order to get
its job done (obviously, since many regular expressions need to backtrack
during their matching). After thinking a bit, I came up with three possible
solutions for the iterator problem.


Considered approaches
---------------------

### The portable

Implement a sliding buffer for reading. I decided not to reinvent the wheel,
and took 'boost::circular_buffer' class and its bidirectional iterators.
However, I was surprised to find out that this class doesn't "just work"
with std::regex. In debug build I got the following creepy assertion:

    /usr/include/boost/circular_buffer/details.hpp:380:
    Assertion `it.is_valid(m_buff)' failed.

Brief investigation had shown that GCC standard library implementation
of regular expressions default-constructs an iterator instance and then
checks it for equality with a valid circular_buffer's iterator. The latter,
in its turn, sees an iterator which doesn't belong to the buffer and
thinks it's invalid. I don't know who is to blame, Boost or the GCC, but
the problem can be solved with

    #define BOOST_CB_DISABLE_DEBUG

before including the corresponding header.


### The not-so-portable

Just 'mmap()' the file and get these bidirectional iterators for free. Now
it's the operating system's headache to fetch and unload pages while
the regex is walking down the memory.

Unfortunately, this solution is:

 - not so portable as I want it to be,
 - unsuitable for matching non-seekable streams.


### The stupid

Since the problem's URL pattern in its original formulation doesn't really
need to backtrack anything (no 'http:' or 'https:' substring can appear in
the middle of a URL), iterator's 'operator --' will actually never be
called by 'std::regex_search'. Let's just provide a dummy operator:

    std::istream_iterator& operator--(std::istream_iterator& a) {
        throw std::brain_damage_exception();
    }

In reality this (obviously) doesn't work. Even though the regex doesn't
need backtracking, the operator got called pretty often. However, it's
possible to derive from the class and implement a rudimentary support for
limited data caching. I played with it a bit, but it turned out to be
rather unproductive. So, this solution is not implemented.


Implementation notes
--------------------

The task formulation leaves a lot of room for creativity. Namely, it doesn't
say which language standard is accepted or whether third-party libraries
are allowed. Since I'm just a human, let me make the task a bit easier to
solve by using:

  - C++11,
  - #pragma once,
  - Boost.

I know, the above things combined can drive some many people insane, but
it's just a test assignment, so it must be tolerant to the experimentation.

In order to make you (== Shodan and co.) especially mad, I decided to use
Boost.Coroutine's yield to return matched substrings one by one. I tried
with Coroutine2, but they broke something in exception propagation, so it
didn't work for me. I thing, I'm going to file a bug report to the devs
some days later.

I think I did somewhere near my best during writing this code, so I will be
very grateful to receive a (reasonably) detailed review about it. Any
critique is welcome, even (especially?) about the grammar and spelling.

--- Pavel Kretov.
