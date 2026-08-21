#include "Gateway.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <sys/event.h>
#include <netinet/tcp.h>

void Gateway::run() {
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(static_cast<std::uint16_t>(port));

	bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	listen(lfd, 16);
	printf("Exchange listening on 127.0.0.1:%d\n", port);
	int kq = kqueue();
	struct kevent ev;
	EV_SET(&ev, lfd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
	kevent(kq, &ev, 1, nullptr, 0, nullptr);
	struct kevent evs[64];
	while (true) {
		int n = kevent(kq, nullptr, 0, evs, 64, nullptr);
		for (int i = 0; i < n; i++) {
			int fd = static_cast<int>(evs[i].ident);
			if (fd == lfd) accept_new(kq, lfd);
			else serve(kq, fd, static_cast<Connection *>(evs[i].udata));
		}
	}
}

void Gateway::dispatch(Connection& conn, MsgType type, const std::byte *body, std::uint16_t len) {
	const std::byte* framed = body - sizeof(MsgHeader);
	std::size_t framed_n = sizeof(MsgHeader) + len;

	switch (type) {
		case MsgType::NewOrder: {
			auto *p = reinterpret_cast<const NewOrderBody*>(body);
			journal.write_order(framed, framed_n);

			Trading::OrderID internal = next_internal++;
			conn.client_to_internal[p->client_oid] = internal;
			if (internal_to_ref.size() <= internal) {
				internal_to_ref.resize(internal+1);
			}
			internal_to_ref[internal] = OrderRef{&conn, p->client_oid};

			Trading::Order o{
				internal,
				p->price,
				p->qty,
				p->qty,
				static_cast<Trading::Side>(p->side),
				static_cast<Trading::OrderType>(p->type)
			};
			auto trades = book.add_order(o);
			append_msg(conn, MsgType::Accepted, AcceptedBody{p->client_oid});
			Trading::Bbo bid = book.best_bid();
			Trading::Bbo ask = book.best_ask();
			std::int64_t bpx = bid.has ? bid.price : 0;
			std::uint32_t bq = bid.has ? bid.qty : 0;
			std::int64_t apx = ask.has ? ask.price : 0;
			std::uint32_t aq = ask.has ? ask.qty : 0;
			for (const auto& t: trades) {
				report_fill(t.bid_trade.aggressor_id, t.bid_trade.price, t.bid_trade.quantity);
				report_fill(t.ask_trade.resting_id, t.ask_trade.price, t.ask_trade.quantity);
				journal.write_trade(p->client_oid, t.bid_trade.price, t.bid_trade.quantity);
				ring.try_push(FeedEvent{FeedType::TradePrint, ++seq,  t.bid_trade.price, t.bid_trade.quantity, p->side, bpx, bq, apx, aq});
			}
			break;
		}
		case MsgType::Cancel: {
			auto *p = reinterpret_cast<const CancelBody*>(body);
			journal.write_order(framed, framed_n);
			auto it = conn.client_to_internal.find(p->client_oid);
			if (it == conn.client_to_internal.end()) {
				append_msg(conn, MsgType::Rejected, RejectedBody{p->client_oid, 1});
				break;
			}
			book.cancel_order(it->second);
			break;
		}
		case MsgType::Modify: {
			auto *p = reinterpret_cast<const ModifyBody*>(body);
			journal.write_order(framed, framed_n);
			auto it = conn.client_to_internal.find(p->client_oid);
			if (it == conn.client_to_internal.end()) {
				append_msg(conn, MsgType::Rejected, RejectedBody{p->client_oid, 1});
				break;
			}
			auto trades = book.modify_order(Trading::OrderModify{it->second,
				p->qty, p->price, static_cast<Trading::Side>(p->side)});
			Trading::Bbo bid = book.best_bid();
			Trading::Bbo ask = book.best_ask();
			std::int64_t bpx = bid.has ? bid.price : 0;
			std::uint32_t bq = bid.has ? bid.qty : 0;
			std::int64_t apx = ask.has ? ask.price : 0;
			std::uint32_t aq = ask.has ? ask.qty : 0;
			for (const auto& t : trades) {
				report_fill(t.bid_trade.aggressor_id, t.bid_trade.price, t.bid_trade.quantity);
				report_fill(t.ask_trade.resting_id, t.ask_trade.price, t.ask_trade.quantity);
				journal.write_trade(p->client_oid, t.bid_trade.price, t.bid_trade.quantity);
				ring.try_push(FeedEvent{FeedType::TradePrint, ++seq, t.bid_trade.price, t.bid_trade.quantity, p->side, bpx, bq, apx, aq});
			}
			break;
		}
		default: break;
	}
	maybe_publish_bbo();
}

template<class Body>
void Gateway::append_msg(Connection& conn, MsgType t, const Body &b) {
	std::byte buf[sizeof(MsgHeader) + sizeof(b)];
	std::size_t n = encode(buf, t, b);
	conn.out_buf.insert(conn.out_buf.end(), buf, buf + n);
}

void Gateway::accept_new(int kq, int lfd) {
	int cfd = accept(lfd, nullptr, nullptr);
	if (cfd < 0) return;
	int yes = 1;
	setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
	auto conn = std::make_unique<Connection>();
	conn->fd = cfd;
	Connection* ptr = conn.get();
	conns[cfd] = std::move(conn);
	struct kevent ev;
	EV_SET(&ev, cfd, EVFILT_READ, EV_ADD, 0, 0, ptr);
	kevent(kq, &ev, 1, nullptr, 0, nullptr);
}

void Gateway::serve(int kq, int fd, Connection *conn) {
	std::byte tmp[4096];
	ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
	if (n <= 0) {
		disconnect(kq, fd, conn);
		return;
	}
	conn->reader.feed(tmp, static_cast<std::size_t>(n));
	MsgType type;
	const std::byte* body;
	std::uint16_t len;
	while (conn->reader.next(type, body, len)) {
		dispatch(*conn, type, body, len);
	}
	flush_all();
}

void Gateway::disconnect(int kq, int fd, Connection *conn) {
	for (auto& [coid, internal] : conn->client_to_internal) {
		book.cancel_order(internal);
		if (internal < internal_to_ref.size()) {
			internal_to_ref[internal].conn = nullptr;
		}
	}
	struct kevent ev;
	EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
	kevent(kq, &ev, 1, nullptr, 0, nullptr);
	close(fd);
	conns.erase(fd);
}

void Gateway::report_fill(Trading::OrderID internal, std::int64_t price, std::uint32_t qty) {
	if (internal >= internal_to_ref.size()) return;
	OrderRef& ref = internal_to_ref[internal];
	if (ref.conn == nullptr) return;
	append_msg(*ref.conn, MsgType::Fill, FillBody{
		ref.client_oid,
		price,
		qty
	});
}

void Gateway::maybe_publish_bbo() {
	Trading::Bbo bid = book.best_bid();
	Trading::Bbo ask = book.best_ask();
	std::int64_t bpx = bid.has ? bid.price : 0;
	std::uint32_t bq = bid.has ? bid.qty : 0;
	std::int64_t apx = ask.has ? ask.price : 0;
	std::uint32_t aq = ask.has ? ask.qty : 0;
	if (bpx != last_bid_px || bq != last_bid_qty || apx != last_ask_px || aq != last_ask_qty) {
		if (ring.try_push(FeedEvent{
			FeedType::BboUpdate,
			++seq,
			0,
			0,
			0,
			bpx,
			bq,
			apx,
			aq
		})) {
			last_bid_px = bpx;
			last_bid_qty = bq;
			last_ask_px = apx;
			last_ask_qty = aq;
		}
	}
}

static void send_all(int fd, std::byte* p, std::size_t n) {
	std::size_t s = 0;
	while (s < n) {
		auto k = send(fd, p + s, n - s, 0);
		if (k <= 0) return;
		s += static_cast<std::size_t>(k);
	}
}

void Gateway::flush_all() {
	for (auto& [fd, c_ptr] : conns) {
		if (c_ptr->out_buf.empty()) continue;
		send_all(c_ptr->fd, c_ptr->out_buf.data(), c_ptr->out_buf.size());
		c_ptr->out_buf.clear();
	}
}
