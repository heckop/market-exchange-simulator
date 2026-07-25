module;

#include<vector>

export module OrderBookTypes;

export namespace Trading{

	using Price = std::int64_t;
	using OrderID = std::uint64_t;
	using Quantity = std::uint32_t;

	enum class OrderType : std::uint8_t {
		GoodTillCancel,
		FillOrKill,
		FillAndKill,
		Market,
		GoodForDay
	};

	enum class Side : std::uint8_t {
		Buy,
		Sell
	};

}



