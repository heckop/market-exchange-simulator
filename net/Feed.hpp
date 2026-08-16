#pragma once
#include <cstdint>
#include <cstddef>
#include <netinet/in.h>

#pragma pack(push, 1)
enum class FeedType : std::uint8_t {
	TradePrint = 1,
	BboUpdate = 2
};

struct FeedHeader {
	std::uint32_t seq;
	std::uint8_t type;
};

struct TradePrintMsg {
	std::int64_t price;
	std::uint32_t qty;
	std::uint8_t aggressor_side;
};

struct BboUpdateMsg {
	std::int64_t bid_px;
	std::uint32_t bid_qty;
	std::int64_t ask_px;
	std::uint32_t ask_qty;
};

#pragma pack(pop)

static_assert(sizeof(FeedHeader) == 5);
static_assert(sizeof(TradePrintMsg) == 13);
static_assert(sizeof(BboUpdateMsg) == 24);

class Feed {
	int sock = -1;
	sockaddr_in group{};
	std::uint32_t seq = 0;
public:
	Feed(const char* group_ip = "239.0.0.1", std::uint16_t port = 9002);
	void publish_trade(std::int64_t price, std::uint32_t qty, std::uint8_t aggressor_side);
	void publish_bbo(std::int64_t bid_px, std::uint32_t bid_qty, std::int64_t ask_px, std::uint32_t ask_qty);
};