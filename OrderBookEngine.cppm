module;

#include<vector>
#include<algorithm>
#include<cstddef>
#include<vector>
#include<algorithm>

export module OrderBookEngine;
export import OrderBookModels;

export namespace Trading {
	class OrderBook {
		struct PooledOrder {
			OrderID id;
			Price price;
			Quantity initial_quantity;
			Quantity remaining_quantity;
			Side side;
			OrderType type;
			std::size_t prev = 0;
			std::size_t next = 0;
			bool active = false;
		};

		struct PriceLevel {
			std::size_t head = 0;
			std::size_t tail = 0;
			Quantity total_qty = 0;
		};

		static constexpr std::size_t NULL_IDX = 0;
		static constexpr std::size_t NUM_TICKS = 100001;
		static constexpr  std::size_t NO_LEVEL = NUM_TICKS;

		std::vector<PooledOrder> order_pool;
		std::vector<PriceLevel> bid_level;
		std::vector<PriceLevel> ask_level;
		std::size_t best_bid_idx = NO_LEVEL;
		std::size_t best_ask_idx = NO_LEVEL;
		Price base_price = 0;
		bool base_set = false;
		Price tick = 100;
		std::size_t out_of_band_count = 0;

		[[nodiscard]] bool in_band(Price price) const {
			if (price >= base_price && base_set && tick_index(price) < NUM_TICKS) {
				return true;
			}
			return false;
		}

		[[nodiscard]] std::size_t tick_index(Price price) const {
			return (price - base_price) / tick;
		}

		void ensure_base(Price first_price) {
			if (base_set) return;
			base_price = first_price - static_cast<Price>(NUM_TICKS/2) * tick;
			order_pool.reserve(5'000'005);
			bid_level.assign(NUM_TICKS, {});
			ask_level.assign(NUM_TICKS, {});
			order_pool.push_back(PooledOrder{});
			base_set = true;
		}


		[[nodiscard]] bool can_fully_fill(Side side, Price price, Quantity qty, OrderType type) const noexcept {
			Quantity filled_qty = 0;
			if (side == Side::Buy) {
				for (std::size_t i = best_ask_idx; i < NUM_TICKS; ++i) {
					if (ask_level[i].head == NULL_IDX) continue;
					Price lp = base_price + static_cast<Price>(i) * tick;
					if (type != OrderType::Market && lp > price) break;
					filled_qty += ask_level[i].total_qty;
					if (filled_qty >= qty) return true;
				}
			} else {
				std::size_t i = best_bid_idx;
				while (i != NO_LEVEL) {
					if (bid_level[i].head != NULL_IDX) {
						Price lp = base_price + static_cast<Price>(i) * tick;
						if (type != OrderType::Market && lp < price) break;
						filled_qty += bid_level[i].total_qty;
						if (filled_qty >= qty) return true;
					}
					if (i == 0) break;
					--i;
				}
			}
			return false;
		}

		void cancel_order_internal(const OrderID id) noexcept {
			if (id >= order_pool.size()) {
				return;
			}
			PooledOrder& o = order_pool[id];
			if (!o.active) return;
			std::size_t idx = tick_index(o.price);
			if (o.side == Side::Buy) bid_level[idx].total_qty -= o.remaining_quantity;
			else ask_level[idx].total_qty -= o.remaining_quantity;
			unlink(o.side, idx, id);
		}

		void ensure_pool(std::size_t order_index) {
			if (order_index >= order_pool.size()) {
				order_pool.resize(order_index+1);
			}
		}

		void advance_best_ask() {
			std::size_t i = best_ask_idx;
			while (i < NUM_TICKS && ask_level[i].head == NULL_IDX) ++i;
			best_ask_idx = (i < NUM_TICKS) ? i : NO_LEVEL;
		}

		void advance_best_bid() {
			std::size_t i = best_bid_idx;
			while (i != NO_LEVEL && bid_level[i].head == NULL_IDX) {
				if (i==0) {
					i = NO_LEVEL;
					break;
				}
				--i;
			}
			best_bid_idx = i;
		}

		void link_at_tail(Side side, std::size_t idx, std::size_t oi) {
			PriceLevel& lvl = (side == Side::Buy ? bid_level : ask_level)[idx];
			order_pool[oi].next = NULL_IDX;
			order_pool[oi].prev = lvl.tail;
			if (lvl.head == NULL_IDX) {
				lvl.head = oi;
			}
			else {
				order_pool[lvl.tail].next = oi;
			}
			lvl.tail = oi;
			order_pool[oi].active = true;
			if (side == Side::Buy) {
				if (best_bid_idx == NO_LEVEL || idx > best_bid_idx) {
					best_bid_idx = idx;
				}
			}
			if (side == Side::Sell) {
				if (best_ask_idx == NO_LEVEL || idx < best_ask_idx) {
					best_ask_idx = idx;
				}
			}
		}

		void unlink(Side side, std::size_t idx, std::size_t oi) {
			PriceLevel& lvl = (side == Side::Buy ? bid_level : ask_level)[idx];
			std::size_t p = order_pool[oi].prev, n = order_pool[oi].next;
			if (p == NULL_IDX) lvl.head = n; else order_pool[p].next = n;
			if (n == NULL_IDX) lvl.tail = p; else order_pool[n].prev = p;
			order_pool[oi].active = false;
			if (lvl.head == NULL_IDX) {
				if (side == Side::Buy && idx == best_bid_idx) advance_best_bid();
				else if (side == Side::Sell && idx == best_ask_idx) advance_best_ask();
			}
		}

	public:
		OrderBook() = default;
		OrderBook(const OrderBook&) = delete;
		OrderBook(const OrderBook&&) = delete;
		OrderBook& operator=(const OrderBook&) = delete;
		OrderBook& operator=(const OrderBook&&) = delete;

		std::vector<Trade> add_order(Order order) {
			ensure_base(order.price);
			if (order.type == OrderType::FillOrKill && !can_fully_fill(order.side, order.price, order.remaining_quantity, order.type)) {
				return{};
			}
			std::vector<Trade> trades;
			if (order.side == Side::Buy) {
				while (order.remaining_quantity > 0 && best_ask_idx != NO_LEVEL) {
					Price level_price = base_price + static_cast<Price>(best_ask_idx)*tick;
					if (order.type != OrderType::Market && order.price < level_price) {
						break;
					}
					PriceLevel& lvl = ask_level[best_ask_idx];
					while (lvl.head != NULL_IDX && order.remaining_quantity > 0) {
						PooledOrder& resting = order_pool[lvl.head];
						Quantity fill_qty = std::min(resting.remaining_quantity, order.remaining_quantity);
						order.remaining_quantity -= fill_qty;
						resting.remaining_quantity -= fill_qty;
						lvl.total_qty -=fill_qty;
						trades.push_back(Trade{
							.bid_trade = TradeInfo{.aggressor_id = order.id, .resting_id = resting.id, .price = level_price, .quantity = fill_qty},
							.ask_trade = TradeInfo{.aggressor_id = order.id, .resting_id = resting.id, .price = level_price, .quantity = fill_qty}
						});
						if (resting.remaining_quantity == 0) {
							unlink(Side::Sell, best_ask_idx, lvl.head);
						}
					}
				}

				if (order.remaining_quantity > 0 && (order.type == OrderType::GoodForDay || order.type == OrderType::GoodTillCancel)) {
					if (in_band(order.price)) {
						std::size_t idx = tick_index(order.price);
						std::size_t oi = order.id;
						ensure_pool(oi);
						order_pool[oi] = PooledOrder{order.id, order.price, order.initial_quantity,
							order.remaining_quantity, order.side, order.type, 0, 0, false};
						bid_level[idx].total_qty += order.remaining_quantity;
						link_at_tail(Side::Buy, idx, oi);
					}
					else {
						++out_of_band_count;
					}
				}
			}
			else {
				while (order.remaining_quantity > 0 && best_bid_idx != NO_LEVEL) {
					Price level_price = base_price + static_cast<Price>(best_bid_idx)*tick;
					if (order.type != OrderType::Market && order.price > level_price) {
						break;
					}
					PriceLevel& lvl = bid_level[best_bid_idx];
					while (lvl.head != NULL_IDX && order.remaining_quantity > 0) {
						PooledOrder& resting = order_pool[lvl.head];
						Quantity fill_qty = std::min(resting.remaining_quantity, order.remaining_quantity);
						order.remaining_quantity -= fill_qty;
						resting.remaining_quantity -= fill_qty;
						lvl.total_qty -=fill_qty;
						trades.push_back(Trade{
							.bid_trade = TradeInfo{.aggressor_id = order.id, .resting_id = resting.id, .price = level_price, .quantity = fill_qty},
							.ask_trade = TradeInfo{.aggressor_id = order.id, .resting_id = resting.id, .price = level_price, .quantity = fill_qty}
						});
						if (resting.remaining_quantity == 0) {
							unlink(Side::Buy, best_bid_idx, lvl.head);
						}
					}
				}

				if (order.remaining_quantity > 0 && (order.type == OrderType::GoodForDay || order.type == OrderType::GoodTillCancel)) {
					if (in_band(order.price)) {
						std::size_t idx = tick_index(order.price);
						std::size_t oi = order.id;
						ensure_pool(oi);
						order_pool[oi] = PooledOrder{order.id, order.price, order.initial_quantity,
							order.remaining_quantity, order.side, order.type, 0, 0, false};
						ask_level[idx].total_qty += order.remaining_quantity;
						link_at_tail(Side::Sell, idx, oi);
					}
					else {
						++out_of_band_count;
					}
				}
			}
			return trades;
		}

		void cancel_order(const OrderID id) noexcept {
			cancel_order_internal(id);
		}

		std::vector<Trade> modify_order(OrderModify modify) {
			if (modify.id >= order_pool.size()) return {};
		    PooledOrder& o = order_pool[modify.id];
			if (!o.active) return {};

		    Quantity filled_qty = o.initial_quantity - o.remaining_quantity;
			if (modify.quantity < filled_qty) return {};

			if (modify.quantity == filled_qty) {
				cancel_order_internal(modify.id);
				return {};
			}

			Quantity net_remaining = modify.quantity - filled_qty;

			if (modify.price != o.price || modify.quantity > o.initial_quantity) {
				cancel_order_internal(modify.id);
				Order pushed_order{
					.id = modify.id,
					.price = modify.price,
					.initial_quantity = modify.quantity,
					.remaining_quantity = net_remaining,
					.side = modify.side,
					.type = OrderType::GoodTillCancel
				};
				return add_order(pushed_order);
			}

			std::size_t idx = tick_index(o.price);
			PriceLevel& lvl = (o.side == Side::Buy ? bid_level : ask_level)[idx];
			lvl.total_qty -= o.remaining_quantity;
			lvl.total_qty += net_remaining;
			o.initial_quantity = modify.quantity;
			o.remaining_quantity = net_remaining;
			return {};
		}

		[[nodiscard]] OrderBookLevelInfos get_level_infos() const {
			OrderBookLevelInfos infos;
			std::size_t idx = best_bid_idx;
			while (idx != NO_LEVEL) {
				if (bid_level[idx].head != NULL_IDX) {
					Price p = base_price + static_cast<Price>(idx) * tick;
					infos.bids.push_back(LevelInfo{.price = p, .quantity = bid_level[idx].total_qty});
				}
				if (idx == 0)break;
				--idx;
			}

			for (std::size_t i = best_ask_idx; i < NUM_TICKS; ++i) {
				if (ask_level[i].head != NULL_IDX) {
					Price p = base_price + static_cast<Price>(i) * tick;
					infos.asks.push_back(LevelInfo{.price = p, .quantity = ask_level[i].total_qty});
				}
			}
			return infos;
		}

		[[nodiscard]] Bbo best_bid() const {
			if (best_bid_idx == NO_LEVEL) {
				return {};
			}
			return {
				.price = base_price + static_cast<Price>(best_bid_idx) * tick,
				.qty = bid_level[best_bid_idx].total_qty,
				.has = true
			};
		}

		[[nodiscard]] Bbo best_ask() const {
			if (best_ask_idx == NO_LEVEL) {
				return {};
			}
			return {
				.price = base_price + static_cast<Price>(best_ask_idx) * tick,
				.qty = ask_level[best_ask_idx].total_qty,
				.has = true
			};
		}
	};
}