#include <cstddef>
#include <netinet/in.h>
#include <sys/socket.h>
#include <vector>
#include <algorithm>
#include <cstdio>
#include "Protocol.hpp"
#include "bench/Timer.hpp"
#include "bench/Histogram.hpp"
#include "bench/OrderGenerator.hpp"

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9001);
	connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	int N = 1'00'000;
	std::vector<std::int64_t> rtts;
	for (int i = 0; i < N; ++i) {
		Action order{
			ActionKind::Add,
			static_cast<std::uint64_t>(i),
			static_cast<std::uint64_t>(10),
			2,
			0,
			0
		};
		std::byte buf[64];
		std::size_t n = 0;
		n = encode(buf, MsgType::NewOrder, NewOrderBody{order.id, order.price, order.quantity, order.side, order.type});
		std::int64_t t0 = bench::now_ns();
		ssize_t k = send(fd, buf, n, 0);
		if (k <= 0) {
			std::printf("Order with id %d send failed", i);
			break;
		}
		FrameReader r;
		bool got = false;
		while (!got) {
			std::byte tmp[64];
			ssize_t k = recv(fd, tmp, sizeof(tmp), 0);
			if (k <= 0) break;
			r.feed(tmp, static_cast<std::size_t>(k));
			MsgType type;
			const std::byte* body;
			std::uint16_t len;
			while (r.next(type, body, len)) {
				if (type == MsgType::Accepted) {
					auto *a = reinterpret_cast<const AcceptedBody*>(body);
					std::int64_t t1 = bench::now_ns();
					if (i >= 1000) rtts.push_back(t1-t0);
					got = true;
				}
			}
		}
	}
	std::sort(rtts.begin(), rtts.end());
	auto pct = [&](double p){ return rtts[static_cast<std::size_t>(p*(rtts.size()-1))]; };
	std::printf("rtt(ns): p50=%lld p99=%lld p99.9=%lld max=%lld\n",
		static_cast<long long>(pct(0.5)),static_cast<long long>(pct(0.99)),static_cast<long long>(pct(0.999)),static_cast<long long>(rtts.back()));
}
