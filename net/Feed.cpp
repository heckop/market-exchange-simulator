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

void Feed::publish_trade(std::uint32_t seq, std::int64_t price, std::uint32_t qty, std::uint8_t aggressor_side) {
	std::byte buf[sizeof(FeedHeader) + sizeof(TradePrintMsg)];
	FeedHeader h{seq, static_cast<std::uint8_t>(FeedType::TradePrint)};
	TradePrintMsg msg{price, qty, aggressor_side};
	std::memcpy(buf, &h, sizeof(h));
	std::memcpy(buf + sizeof(h), &msg, sizeof(msg));
	sendto(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&group), sizeof(group));
}

void Feed::publish_bbo(std::uint32_t seq, std::int64_t bid_px, std::uint32_t bid_qty, std::int64_t ask_px, std::uint32_t ask_qty) {
	std::byte buf[sizeof(FeedHeader) + sizeof(BboUpdateMsg)];
	FeedHeader h{seq, static_cast<std::uint8_t>(FeedType::BboUpdate)};
	BboUpdateMsg msg{bid_px, bid_qty, ask_px, ask_qty};
	std::memcpy(buf, &h, sizeof(h));
	std::memcpy(buf + sizeof(h), &msg, sizeof(msg));
	sendto(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&group), sizeof(group));
}

void Feed::send_batch(const FeedEvent *evs, std::size_t n) {
	std::byte buf[MAX_DATAGRAM];
	std::size_t buf_used = 0;
	auto flush = [&] {
		if (buf_used) {
			sendto(sock, buf, buf_used, 0, reinterpret_cast<sockaddr*>(&group), sizeof(group));
			buf_used = 0;
		}
	};
	for (std::size_t i = 0; i < n; ++i) {
		std::size_t msg = sizeof(FeedHeader) + (evs[i].kind == FeedType::BboUpdate ? sizeof(BboUpdateMsg) : sizeof(TradePrintMsg));
		if (buf_used + msg > MAX_DATAGRAM) {
			flush();
		}
		FeedHeader h{evs[i].seq, static_cast<std::uint8_t>(evs[i].kind)};
		std::memcpy(buf + buf_used, &h, sizeof(h));
		buf_used += sizeof(h);
		if (evs[i].kind == FeedType::TradePrint) {
			TradePrintMsg m{evs[i].price, evs[i].qty, evs[i].side};
			std::memcpy(buf + buf_used, &m, sizeof(m));
			buf_used += sizeof(m);
		}
		else {
			BboUpdateMsg m{evs[i].bid_px, evs[i].bid_qty, evs[i].ask_px, evs[i].ask_qty};
			std::memcpy(buf + buf_used, &m, sizeof(m));
			buf_used += sizeof(m);
		}
	}
	flush();
}
