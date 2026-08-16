#include "Feed.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstddef>

int main() {
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	int yes = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(9002);
	bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	ip_mreq mreq{};
	inet_pton(AF_INET, "239.0.0.1", &mreq.imr_multiaddr);
	inet_pton(AF_INET, "127.0.0.1", &mreq.imr_interface);
	setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

	std::byte buf[1500];
	std::uint32_t expected = 1;
	std::uint64_t gaps = 0, trades = 0, bbos = 0;
	while (true) {
		ssize_t n = recvfrom(s, buf, sizeof(buf), 0, nullptr, nullptr);
		if (n < static_cast<ssize_t>(sizeof(FeedHeader)))continue;
		FeedHeader h;
		std::memcpy(&h, buf, sizeof(h));
		if (h.seq != expected) {
			gaps += h.seq - expected;
			std::printf("GAP : expected sequence number %u got %u\n", expected, h.seq);
		}
		expected = h.seq + 1;
		if (h.type == static_cast<std::uint8_t>(FeedType::TradePrint)) {
			TradePrintMsg msg;
			std::memcpy(&msg, buf + sizeof(h), sizeof(msg));
			++trades;
			std::printf("TRADE seq = %u, px = %lld, qty = %u, side = %u\n", h.seq, static_cast<long long>(msg.price), msg.qty, msg.aggressor_side);
		}
		else if (h.type == static_cast<std::uint8_t>(FeedType::BboUpdate)) {
			BboUpdateMsg msg;
			std::memcpy(&msg, buf + sizeof(h), sizeof(msg));
			++bbos;
			std::printf("BBO seq = %u, bid = %lld/%u, ask = %lld/%u\n", h.seq, static_cast<long long>(msg.bid_px), msg.bid_qty, static_cast<long long>(msg.ask_px), msg.ask_qty);
		}
	}
}