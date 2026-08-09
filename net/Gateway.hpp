#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "Protocol.hpp"
#include "Journal.hpp"

import OrderBookEngine;

class Gateway {
	int port;
	Trading::OrderBook& book;
	Journal& journal;
	FrameReader reader;
	std::unordered_map<std::uint64_t, Trading::OrderID> client_to_internal;
	std::vector<std::uint64_t> internal_to_client;
	Trading::OrderID next_internal = 1;
	std::vector<std::byte> out_buf;
	int cfd = -1;

	void dispatch(MsgType type, const std::byte* body, std::uint16_t len);

	template <class Body> void send_msg(MsgType t, const Body& b);

public:
	Gateway(int port, Trading::OrderBook& book, Journal& journal)
	: port(port), book(book), journal(journal) {}
	void run_once();
};
