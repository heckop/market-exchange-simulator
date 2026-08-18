#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <array>

template<class T, std::size_t N>
class SpscRing {
	static_assert((N & (N-1)) == 0, "N must be a power of 2");
	std::array<T, N> buf;
	alignas(64) std::atomic<std::size_t> head{0};
	alignas(64) std::atomic<std::size_t> tail{0};
public:
	bool try_push(const T& v) {
		auto h = head.load(std::memory_order_relaxed);
		auto t = tail.load(std::memory_order_acquire);
		if (h - t == N) return false;
		buf[h & (N-1)] = v;
		head.store(h+1, std::memory_order_release);
		return true;
	}

	bool try_pop(T& out) {
		auto t = tail.load(std::memory_order_relaxed);
		auto h = head.load(std::memory_order_acquire);
		if (h == t) return false;
		out = buf[t & (N-1)];
		tail.store(t+1, std::memory_order_release);
		return true;
	}
};