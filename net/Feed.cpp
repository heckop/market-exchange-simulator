#include "Feed.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>

Feed::Feed(const char *group_ip, std::uint16_t port) {
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	unsigned char loop = 1, ttl = 1;
	in_addr mif{};
	inet_pton(AF_INET, "127.0.0.1", &mif);
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mif, sizeof(mif));
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	group.sin_family = AF_INET;
	group.sin_port = htons(port);
	inet_pton(AF_INET, group_ip, &group.sin_addr);
}

void Feed::publish_trade(std::int64_t price, std::uint32_t qty, std::uint8_t aggressor_side) {
	std::byte buf[sizeof(FeedHeader) + sizeof(TradePrintMsg)];
	FeedHeader h{++seq, static_cast<std::uint8_t>(FeedType::TradePrint)};
	TradePrintMsg msg{price, qty, aggressor_side};
	std::memcpy(buf, &h, sizeof(h));
	std::memcpy(buf + sizeof(h), &msg, sizeof(msg));
	sendto(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&group), sizeof(group));
}

void Feed::publish_bbo(std::int64_t bid_px, std::uint32_t bid_qty, std::int64_t ask_px, std::uint32_t ask_qty) {
	std::byte buf[sizeof(FeedHeader) + sizeof(BboUpdateMsg)];
	FeedHeader h{++seq, static_cast<std::uint8_t>(FeedType::BboUpdate)};
	BboUpdateMsg msg{bid_px, bid_qty, ask_px, ask_qty};
	std::memcpy(buf, &h, sizeof(h));
	std::memcpy(buf + sizeof(h), &msg, sizeof(msg));
	sendto(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&group), sizeof(group));
}
