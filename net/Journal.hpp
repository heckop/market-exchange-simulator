#pragma once
#include<cstdint>
#include<cstddef>
#include<string>
#include<fstream>

class Journal {
	std::ofstream os;
public:
	explicit Journal(const std::string& path);
	void write_order(const std::byte* framed, std::size_t n);
	void write_trade(std::uint64_t client_oid, std::int64_t price, std::uint32_t qty);
	~Journal();
};