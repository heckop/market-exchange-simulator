#include "Gateway.hpp"
#include "SpscRing.hpp"
#include <thread>

int main() {
	Trading::OrderBook book;
	Journal journal("exchange.journal");
	SpscRing<FeedEvent, FEED_RING_SIZE> ring;
	Feed feed;
	std::thread publisher(
		[&]{
			FeedEvent batch[64];
			while (true) {
				std::size_t n = 0;
				while (n < 64 && ring.try_pop(batch[n])) {
					++n;
				}
				if (n) feed.send_batch(batch, n);
			}
		}
		);
	Gateway gw(9001, book, journal, ring);
	gw.run();
	return 0;
}
