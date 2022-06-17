#pragma once

#include <map>
#include <chrono>

using f32 = float;
using f64 = double;
using i64 = int64_t;
using u64 = uint64_t;
using u32 = uint32_t;
using i32 = int32_t;
using u16 = uint16_t;
using i16 = int16_t;
using u8 = uint8_t;
using i8 = int8_t;
using c8 = const i8;
using b8 = bool; 

inline void hashCombine(std::size_t& seed, size_t other) {
	seed ^= other + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// the cherno's scope timer
class TimerCollection
{
public:
	template<typename Callback>
	void forEach(Callback callback)
	{
		for (auto& pair : times)
		{
			callback(pair);
		}
	}

	void submitTime(const char* name, float time)
	{
		times[name] = time;
	}

	static TimerCollection* get()
	{
		if (collection == nullptr)
			collection = new TimerCollection();
		return collection;
	}
private:
	static TimerCollection* collection;
	std::map<const char*, float> times;
};

class ScopeTimer
{
	std::chrono::nanoseconds m_Time;
	const char* m_Name;
public:
	ScopeTimer(const char* name)
		: m_Name(name)
	{
		m_Time = std::chrono::high_resolution_clock::now().time_since_epoch();
	}

	~ScopeTimer()
	{
		m_Time = std::chrono::high_resolution_clock::now().time_since_epoch() - m_Time;
		TimerCollection::get()->submitTime(m_Name, m_Time.count() / 1000.0f); // convert to micro seconds
	}
};
