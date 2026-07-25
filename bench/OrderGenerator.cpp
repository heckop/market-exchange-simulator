#include<vector>
#include "OrderGenerator.hpp"

OrderGenerator::OrderGenerator(const Scenario &s) : scn(s), rng(s.seed), mid(s.start_mid) {}

std::vector<Action> OrderGenerator::generate() {
	std::uniform_real_distribution<double> urnd(0,1);
	std::uniform_int_distribution<uint32_t> uing(1, 500);
	std::exponential_distribution<double> exp_rng(1.0/scn.price_spread_ticks);

	auto price_for = [&](std::uint8_t side, std::uint8_t type) -> std::int64_t {
		auto dist = static_cast<std::int64_t>(exp_rng(rng));
		bool marketable = (type != 0) || (urnd(rng) < scn.marketable_frac);
		std::int64_t p = mid;
		if (!marketable) p += (side == 0) ? -(dist * scn.tick) : (dist * scn.tick);
		else             p += (side == 0) ?  (dist * scn.tick) : -(dist * scn.tick);
		return p;
	};

	auto add_order = [&]() -> Action {
		const std::uint64_t id = next_id++;
		const std::uint8_t side = urnd(rng) < 0.5 ? 0 : 1;
		double p_order = urnd(rng);
		std::uint8_t type;
		if (p_order < scn.p_market) {
			type = 3;
		}
		else if (p_order < scn.p_market + scn.p_fok) {
			type = 1;
		}
		else if (p_order < scn.p_market + scn.p_fok + scn.p_fak) {
			type = 2;
		}
		else type = 0;
		auto price = price_for(side, type);
		live_ids.push_back(LiveOrder{id, side, type});
		return Action{ActionKind::Add, id, price, uing(rng), side, type};
	};
	std::vector<Action> orderList;
	orderList.reserve(scn.num_orders);
	for (std::uint64_t i=0;i<scn.num_orders;i++) {
		double p_tick = urnd(rng);
		if (p_tick < scn.p_mid_move) {
			int tick_direction = urnd(rng) < 0.5 ? -1 : 1;
			mid += tick_direction * scn.tick;
		}
		double p = urnd(rng);
		if (p < scn.p_add) {
			orderList.push_back(add_order());
		}
		else if (p < scn.p_add + scn.p_cancel) {
			if (live_ids.empty()) {
				orderList.push_back(add_order());
				continue;
			}
			std::uniform_int_distribution<std::size_t> index(0, live_ids.size()-1);
			std::size_t idx = index(rng);
			std::uint64_t order_id = live_ids[idx].id;
			live_ids[idx] = live_ids.back();
			live_ids.pop_back();
			orderList.push_back(Action{ActionKind::Cancel, order_id, 0, 0, 0, 0 });

		}
		else {
			if (live_ids.empty()) {
				orderList.push_back(add_order());
				continue;
			}
			std::uniform_int_distribution<std::size_t> index(0, live_ids.size()-1);
			std::size_t idx = index(rng);
			std::uint64_t order_id = live_ids[idx].id;
			std::uint8_t side = live_ids[idx].side;
			std::uint8_t type = live_ids[idx].type;
			auto price = price_for(side, type);
			orderList.push_back(Action{ActionKind::Modify, order_id, price, uing(rng), side, type});
		}
	}

	return orderList;
}
