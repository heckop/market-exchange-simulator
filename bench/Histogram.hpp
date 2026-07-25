#pragma once
#include <array>
#include <algorithm>
#include<cmath>
#include<cstddef>
#include <cstdint>


class Histogram {
public:
	void record(std::int64_t ns) noexcept;
	std::int64_t percentile(double p) const;
	std::int64_t max() const noexcept;
	std::int64_t min() const noexcept;
	std::uint64_t count() const noexcept;
	std::uint64_t overflow_count() const noexcept;
private:
	static constexpr std::int64_t WIDTH_NS = 10;
	static constexpr std::size_t N = 6554;
	std::array<std::uint64_t, N> buckets{};
	std::int64_t max_l = 0;
	std::int64_t min_l = INT64_MAX;
	std::uint64_t count_l = 0;
};

inline void Histogram::record(std::int64_t ns) noexcept{
	if (ns < 0) {
		return;
	}
	std::int64_t idx = ns/WIDTH_NS;
	if (idx >= N-1) {
		idx = N-1;
	}
	count_l++;
	max_l = std::max(max_l, ns);
	min_l = std::min(min_l, ns);
	buckets[idx]++;
}

inline std::int64_t Histogram::percentile(double p) const {
	if (count_l == 0) return 0;
	if (p == 1.0) {
		return max_l;
	}
	double rank = std::ceil(p*count_l);
	double cumulative = 0;
	for (std::size_t idx = 0; idx < N; ++idx) {
		cumulative += buckets[idx];
		if (cumulative >= rank) {
			return (idx == N-1) ? max_l : static_cast<std::int64_t>(idx) * WIDTH_NS;
		}
	}

	return max_l;
}

inline std::int64_t Histogram::max() const noexcept {
	return max_l;
}

inline std::int64_t Histogram::min() const noexcept {
	return min_l;
}

inline std::uint64_t Histogram::count() const noexcept {
	return count_l;
}

inline std::uint64_t Histogram::overflow_count() const noexcept {
	return buckets[N-1];
}
