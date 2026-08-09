#pragma once

#include<cstdint>
#include<cstddef>
#include<vector>
#include<cstring>

enum class MsgType : std::uint8_t {
	NewOrder = 1,
	Cancel = 2,
	Modify = 3,
	Accepted = 101,
	Fill = 102,
	Rejected = 103
};

#pragma pack(push, 1)
struct MsgHeader {
	std::uint16_t len;
	std::uint8_t type;
	std::uint8_t pad;
};

struct NewOrderBody {
	std::uint64_t client_oid;
	std::int64_t price;
	std::uint32_t qty;
	std::uint8_t side;
	std::uint8_t type;
};

struct CancelBody {
	std::uint64_t client_oid;
};

struct ModifyBody {
	std::uint64_t client_oid;
	std::int64_t price;
	std::uint32_t qty;
	std::uint8_t side;
};

struct AcceptedBody {
	std::uint64_t client_oid;
};

struct FillBody {
	std::uint64_t client_oid;
	std::int64_t price;
	std::uint32_t qty;
};

struct RejectedBody {
	std::uint64_t client_oid;
	std::uint8_t reason;
};
#pragma pack(pop)

static_assert(sizeof(MsgHeader) == 4);
static_assert(sizeof(NewOrderBody) == 22);
static_assert(sizeof(CancelBody) == 8);
static_assert(sizeof(ModifyBody) == 21);
static_assert(sizeof(AcceptedBody) == 8);
static_assert(sizeof(FillBody) == 20);
static_assert(sizeof(RejectedBody) == 9);

class FrameReader {
	std::vector<std::byte> buf;
	std::size_t consumed = 0;
public:
	void feed(const std::byte* data, std::size_t n) {
		buf.insert(buf.end(), data, data+n);
	}

	bool next(MsgType& type, const std::byte*& body, std::uint16_t& body_len) {
		if (consumed > 4096) {
			buf.erase(buf.begin(), buf.begin()+consumed);
			consumed = 0;
		}
		std::size_t available = buf.size() - consumed;
		if (available < sizeof(MsgHeader)) {
			return false;
		}
		MsgHeader header = {};
		std::memcpy(&header, buf.data()+consumed, sizeof(MsgHeader));
		std::size_t total = sizeof(MsgHeader) + header.len;
		if (available < total) {
			return false;
		}
		type = static_cast<MsgType>(header.type);
		body = buf.data() + consumed + sizeof(MsgHeader);
		body_len = header.len;
		consumed += total;
		return true;
	}
};

template<typename  Body>
std::size_t encode(std::byte* out, MsgType t, const Body& b) {
	MsgHeader h{
		static_cast<std::uint16_t>(sizeof(Body)),
		static_cast<std::uint8_t>(t),
		0
	};
	std::memcpy(out, &h, sizeof(h));
	std::memcpy(out + sizeof(h), &b, sizeof(b));
	return sizeof(h) + sizeof(b);
}

