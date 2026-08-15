module;
#include <fstream>
export module OrderBookModels;

export import OrderBookTypes;


export namespace Trading {
	struct Order {
		OrderID id;
		Price price;
		Quantity initial_quantity;
		Quantity remaining_quantity;
		Side side;
		OrderType type;

		[[nodiscard]] bool isFilled() const noexcept {
			return remaining_quantity == 0;
		}

		void fill(const Quantity qty) noexcept {
			if (remaining_quantity < qty) {
				remaining_quantity = 0;
			}
			else {
				remaining_quantity -= qty;
			}
		}
	};

	struct OrderModify {
		OrderID id;
		Quantity quantity;
		Price price;
		Side side;
	};

	struct TradeInfo {
		OrderID aggressor_id = 0;
		OrderID resting_id = 0;
		Price price;
		Quantity quantity;
	};

	struct Trade {
		TradeInfo bid_trade;
		TradeInfo ask_trade;
	};

	struct LevelInfo {
		Price price;
		Quantity quantity;
	};

	struct OrderBookLevelInfos {
		std::vector<LevelInfo> bids;
		std::vector<LevelInfo> asks;
	};

	struct Bbo {
		Price price = 0;
		Quantity qty = 0;
		bool has = false;
	};

}
