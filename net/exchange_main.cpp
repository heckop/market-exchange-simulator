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
			FeedEvent ev;
			while (true) {
				if (ring.try_pop(ev)) {
					if (ev.kind == FeedType::TradePrint) {
						feed.publish_trade(ev.seq, ev.price, ev.qty, ev.side);
					}
					else {
						feed.publish_bbo(ev.seq, ev.bid_px, ev.bid_qty, ev.ask_px, ev.ask_qty);
					}
				};
			}
		}
		);
	Gateway gw(9001, book, journal, ring);
	gw.run();
	return 0;
}
