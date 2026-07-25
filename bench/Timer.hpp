#pragma once
#include<cstdint>
#include<chrono>

namespace bench {
	inline std::int64_t now_ns() noexcept {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}

