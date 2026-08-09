#include "Gateway.hpp"

int main() {
	Trading::OrderBook book;
	Journal journal("exchange.journal");
	Gateway gw(9001, book, journal);
	gw.run_once();
	return 0;
}