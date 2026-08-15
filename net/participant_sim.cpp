#include "Protocol.hpp"
#include "../bench/OrderGenerator.hpp"
#include "../bench/Timer.hpp"
#include "../bench/Histogram.hpp"
import OrderBookEngine;
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <thread>
#include <atomic>

static void send_all(int fd, const std::byte* p, std::size_t n) {
	std::size_t s = 0;
	while (s < n) {
		ssize_t k = send(fd, p+s, n-s, 0);
		if (k <= 0) {
			return;
		}
		s += static_cast<std::size_t>(k);
	}
}

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(9001);
	connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

	Scenario scn;
	scn.num_orders = 5'000'000;
	std::vector<std::int64_t> send_ts(scn.num_orders+2, 0);
	Histogram rtt;
	std::vector<std::pair<std::int64_t, std::uint32_t>> got;
	std::vector<std::int64_t> rtts;
	std::thread reader([&] {
		FrameReader r;
		std::byte tmp[4096];
		while (true) {
			ssize_t k = recv(fd, tmp, sizeof(tmp), 0);
			if (k <= 0) break;

			r.feed(tmp, static_cast<std::size_t>(k));
			MsgType type;
			const std::byte* body;
			std::uint16_t len;
			while (r.next(type, body, len)) {
				if (type == MsgType::Accepted) {
					auto *a = reinterpret_cast<const AcceptedBody*>(body);
					rtts.push_back(bench::now_ns() - send_ts[a->client_oid]);
					rtt.record(bench::now_ns() - send_ts[a->client_oid]);
				}
				if (type == MsgType::Fill) {
					auto* f = reinterpret_cast<const FillBody*>(body);
					got.push_back({f->price, f->qty});
				}
			}
		}
	});

	OrderGenerator gen(scn);
	auto actions = gen.generate();

	Trading::OrderBook oracle;
	std::vector<std::pair<std::int64_t, std::uint32_t>> expected;
	for (const auto& a : actions) {
		std::vector<Trading::Trade> tr;
		if (a.kind == ActionKind::Add) {
			tr = oracle.add_order(Trading::Order{a.id, a.price, a.quantity, a.quantity,
			static_cast<Trading::Side>(a.side), static_cast<Trading::OrderType>(a.type)});
		}
		else if (a.kind == ActionKind::Cancel) {
			oracle.cancel_order(a.id);
		}
		else tr = oracle.modify_order(Trading::OrderModify{
			a.id,
			a.quantity,
			a.price,
			static_cast<Trading::Side>(a.side)
		});

		for (const auto& t : tr) {
			expected.emplace_back(
			t.bid_trade.price,
			t.bid_trade.quantity);
		}
	}
	std::int64_t t0 = bench::now_ns();
	for (const auto& a : actions) {
		std::byte buf[64];
		std::size_t n = 0;
		if (a.kind == ActionKind::Add) {
			send_ts[a.id] = bench::now_ns();
			n = encode(buf, MsgType::NewOrder, NewOrderBody{a.id, a.price, a.quantity, a.side, a.type});
		}
		else if (a.kind == ActionKind::Cancel) {
			n = encode(buf, MsgType::Cancel, CancelBody{a.id});
		}
		else n = encode(buf, MsgType::Modify, ModifyBody{a.id, a.price, a.quantity, a.side});

		send_all(fd, buf, n);
	}
	shutdown(fd, SHUT_WR);
	reader.join();

	std::int64_t t1 = bench::now_ns();

	bool ok = (got == expected);
	std::printf("correctness: %s (%zu fills vs %zu expected)\n", ok ? "PASS" : "FAIL", got.size(), expected.size());
	std::ranges::sort(rtts);
	auto pct = [&](double p){ return rtts.empty() ? 0 :
	rtts[static_cast<std::size_t>(p * (rtts.size() - 1))]; };
	std::printf("rtt(ns): p50=%lld p99=%lld p99.9=%lld max=%lld\n", (long long)pct(0.5), (long long)pct(0.99), (long long)pct(0.999),(long long)rtts.back());
	double sec = static_cast<double>(t1 - t0) / 1e9;
	std::printf("throughput: %.0f orders/sec over %zu orders\n", actions.size() / sec, actions.size());
	std::printf("rtt(ns): p50=%lld p99=%lld p99.9=%lld max=%lld\n",
		static_cast<long long>(rtt.percentile(0.5)),
		static_cast<long long>(rtt.percentile(0.99)),
		static_cast<long long>(rtt.percentile(0.999)),
		static_cast<long long>(rtt.max()));
	close(fd);
}
