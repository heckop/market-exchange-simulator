#pragma once
#include <cstdint>
#include<cmath>

struct Scenario {
	std::uint64_t num_orders = 5'000'000;
	std::uint64_t seed = 42;
	// probabilities of order type
	double p_add = 0.55;
	double p_cancel = 0.35;
	double p_modify = 0.10;

	double p_market = 0.10;
	double p_fak = 0.03;
	double p_fok = 0.02;

	std::int64_t start_mid = 5'00'000;
	std::int64_t tick = 100;
	double p_mid_move = 0.05;
	double marketable_frac = 0.15;
	double price_spread_ticks = 20.0;

	[[nodiscard]] inline bool valid() const;
};

inline Scenario default_data() {
	return Scenario{};
}

[[nodiscard]] inline bool Scenario::valid() const {
	bool valid = true;
	if (std::abs(p_modify+p_add+p_cancel - 1.0) < 1e-9) valid = false;
	if (p_market+p_fok+p_fak>1) valid = false;
	return valid;
}
