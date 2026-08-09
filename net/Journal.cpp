#include "Journal.hpp"

Journal::Journal(const std::string &path) {
	os.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
}

void Journal::write_order(const std::byte *framed, std::size_t n) {
	os.write(reinterpret_cast<const char*>(framed), n);
}

void Journal::write_trade(std::uint64_t client_oid, std::int64_t price, std::uint32_t qty) {
	std::uint8_t tag = 'T';
	os.write(reinterpret_cast<const char*>(&tag), 1);
	os.write(reinterpret_cast<const char*>(&client_oid), sizeof(client_oid));
	os.write(reinterpret_cast<const char*>(&price), sizeof(price));
	os.write(reinterpret_cast<const char*>(&qty), sizeof(qty));
}

Journal::~Journal() {
	os.flush();
}
