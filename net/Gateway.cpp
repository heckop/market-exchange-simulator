#include "Gateway.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>

void Gateway::run_once() {
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(static_cast<std::uint16_t>(port));

	bind(lfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	listen(lfd, 1);
	printf("Exchange listening on 127.0.0.1:%d\n", port);
	cfd = accept(lfd, nullptr, nullptr);
	std::byte tmp[4096];
	while (true) {
		ssize_t n = recv(cfd, tmp, sizeof(tmp), 0);
		if (n <= 0) break;

		reader.feed(tmp, static_cast<size_t>(n));
		MsgType type;
		const std::byte* body;
		std::uint16_t len;
		while (reader.next(type, body, len)) {
			dispatch(type, body, len);
		}
	}
	close(cfd);
	close(lfd);
}

void Gateway::dispatch(MsgType type, const std::byte *body, std::uint16_t len) {
	const std::byte* framed = body - sizeof(MsgHeader);
	std::size_t framed_n = sizeof(MsgHeader) + len;

	switch (type) {
		case MsgType::NewOrder: {
			auto *p = reinterpret_cast<const NewOrderBody*>(body);
			journal.write_order(framed, framed_n);

			Trading::OrderID internal = next_internal++;
			client_to_internal[p->client_oid] = internal;
			if (internal_to_client.size() <= internal) {
				internal_to_client.resize(internal+1);
			}
			internal_to_client[internal] = p->client_oid;

			Trading::Order o{
				internal,
				p->price,
				p->qty,
				p->qty,
				static_cast<Trading::Side>(p->side),
				static_cast<Trading::OrderType>(p->type)
			};
			auto trades = book.add_order(o);
			send_msg(MsgType::Accepted, AcceptedBody{p->client_oid});

			for (const auto& t: trades) {
				send_msg(MsgType::Fill, FillBody{p->client_oid, t.bid_trade.price, t.bid_trade.quantity});
				journal.write_trade(p->client_oid, t.bid_trade.price, t.bid_trade.quantity);
			}
			break;
		}
		case MsgType::Cancel: {
			auto *p = reinterpret_cast<const CancelBody*>(body);
			journal.write_order(framed, framed_n);
			auto it = client_to_internal.find(p->client_oid);
			if (it == client_to_internal.end()) {
				send_msg(MsgType::Rejected, RejectedBody{p->client_oid, 1});
				break;
			}
			book.cancel_order(it->second);
			break;
		}
		case MsgType::Modify: {
			auto *p = reinterpret_cast<const ModifyBody*>(body);
			journal.write_order(framed, framed_n);
			auto it = client_to_internal.find(p->client_oid);
			if (it == client_to_internal.end()) {
				send_msg(MsgType::Rejected, RejectedBody{p->client_oid, 1});
				break;
			}
			auto trades = book.modify_order(Trading::OrderModify{it->second,
				p->qty, p->price, static_cast<Trading::Side>(p->side)});
			for (const auto& t : trades) {
				send_msg(MsgType::Fill, FillBody{p->client_oid, t.bid_trade.price, t.bid_trade.quantity});
				journal.write_trade(p->client_oid, t.bid_trade.price, t.bid_trade.quantity);
			}
			break;
		}
		default: break;
	}
}

template<class Body>
void Gateway::send_msg(MsgType t, const Body &b) {
	std::byte buf[sizeof(MsgHeader) + sizeof(b)];
	std::size_t n = encode(buf, t, b);
	ssize_t sent = send(cfd, buf, n, 0);
	(void)sent;
}
