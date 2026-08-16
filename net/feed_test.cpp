#include "Feed.hpp"
#include <unistd.h>
int main() {
	Feed feed;
	for (int i = 0; i < 20; ++i) {
		feed.publish_trade(500000 + i * 100, 10, 0);
		usleep(200000);
	}
}