#include "Gateway.hpp"

int main() {
	Trading::OrderBook book;
	Journal journal("exchange.journal");
	Feed feed;
	Gateway gw(9001, book, journal, feed);
	gw.run();
	return 0;
}