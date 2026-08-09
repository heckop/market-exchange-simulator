#include<cassert>
#include <iostream>
#include <iterator>

#include "Protocol.hpp"

int main() {
	std::byte wire[128];
	std::size_t w = 0;
	NewOrderBody n{42,500000, 100, 0, 0};
	CancelBody c{42};
	w += encode(wire+w, MsgType::NewOrder, n);
	w += encode(wire+w, MsgType::Cancel, c);

	FrameReader r;
	MsgType t;
	const std::byte* body;
	std::uint16_t len;
	int got = 0;
	for (std::size_t i =0; i < w; ++i) {
		r.feed(wire+i, 1);
		while (r.next(t, body, len)) {
			if (got == 0) {
				assert(t == MsgType::NewOrder);
				auto* p = reinterpret_cast<const NewOrderBody*>(body);
				assert(p->client_oid == 42 && p->price == 500000 && p->qty == 100);
			}
			else {
				assert(t == MsgType::Cancel);
				assert(reinterpret_cast<const CancelBody*>(body)->client_oid == 42);
			}
			++got;
		}
		assert(got == 2);
	}

	r.feed(wire, w);
	assert(r.next(t, body, len) == true);
	assert(r.next(t, body, len) == true);
	assert(r.next(t, body, len) == false);
	std::cout<<"Protocol working fine\n";
	return 0;
}
