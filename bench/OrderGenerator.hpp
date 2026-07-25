#pragma once
#include<cstdint>
#include <random>
#include <vector>

#include "Scenario.hpp"

enum class ActionKind : std::uint8_t {
	Add,
	Cancel,
	Modify
};

struct Action {
	ActionKind kind;
	std::uint64_t id;
	std::int64_t price;
	std::uint32_t quantity;
	std::uint8_t side;
	std::uint8_t type;
};

struct LiveOrder {
	std::uint64_t id;
	std::uint8_t side;
	std::uint8_t type;
};

class OrderGenerator {
public:
	explicit OrderGenerator(const Scenario& s);
	std::vector<Action> generate();
private:
	Scenario scn;
	std::mt19937_64 rng;
	std::int64_t mid;
	std::uint64_t next_id = 1;
	std::vector<LiveOrder> live_ids;
};