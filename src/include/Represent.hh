#ifndef __REPRESENT_HH__
#define __REPRESENT_HH__

#include <memory>
#include <utility>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define iferr(condition) if (unlikely(condition))
#define ifsucc(condition) if (likely(condition))

typedef unsigned char byte;

template <typename T>
using Smart = std::shared_ptr<T>;

template <typename T>
using UniqueSmart = std::unique_ptr<T>;

template <typename T>
using WeakSmart = std::weak_ptr<T>;

#endif
