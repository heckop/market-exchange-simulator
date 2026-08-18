#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "Protocol.hpp"
#include "Journal.hpp"
#include "Feed.hpp"
#include "SpscRing.hpp"

import OrderBookEngine;

struct Connection {
	int fd = -1;
	FrameReader reader;
	std::unordered_map<std::uint64_t, Trading::OrderID> client_to_internal;
};

struct OrderRef {
	Connection* conn = nullptr;
	std::uint64_t client_oid = 0;
};

class Gateway {
	int port;
	Trading::OrderBook& book;
	Journal& journal;
	std::unordered_map<int, std::unique_ptr<Connection>> conns;
	std::vector<OrderRef> internal_to_ref;
	Trading::OrderID next_internal = 1;
	std::vector<std::byte> out_buf;
	int cfd = -1;
	SpscRing<FeedEvent, FEED_RING_SIZE>& ring;
	std::int64_t last_bid_px = 0;
	std::uint32_t last_bid_qty = 0;
	std::int64_t last_ask_px = 0;
	std::uint32_t last_ask_qty = 0;
	std::uint32_t seq = 0;

	template <class Body> void send_msg(int fd, MsgType t, const Body& b);
	void accept_new(int kq, int lfd);
	void serve(int kq, int fd, Connection* conn);
	void disconnect(int kq, int fd, Connection* conn);
	void dispatch(Connection& conn, MsgType msg, const std::byte* body, std::uint16_t len);
	void report_fill(Trading::OrderID internal, std::int64_t price, std::uint32_t qty);
	void maybe_publish_bbo();
public:
	Gateway(int port, Trading::OrderBook& book, Journal& journal, SpscRing<FeedEvent, FEED_RING_SIZE>& ring)
	: port(port), book(book), journal(journal), ring(ring) {}
	void run();
};
