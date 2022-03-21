#pragma once
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <functional>


namespace yc
{
	using byte_t = unsigned char;
	using socket_t = unsigned __int64;
}

namespace yc_packet_event_sp
{
	static std::unordered_map<int, std::list<std::function<void(void*, yc::socket_t)>>> event;
	static std::unordered_map<size_t, int> packet_events;
}

#pragma pack(push, 1)
union int_to_byte
{
	int i;
	char b[sizeof(int)];
};
union size_t_to_byte
{
	size_t i;
	char b[sizeof(size_t)];
};

class PacketEvent;
template <typename T>
union packet_t
{
	packet_t() { memset(this, 0, sizeof(packet_t<T>)); }
	T value;
private:
	yc::byte_t byte_data[sizeof(T)];
public:
	const yc::byte_t* ToByte()
	{
		return byte_data;
	}

	int pack(unsigned char* buffer)
	{
		using namespace yc_packet_event_sp;
		auto curBuffer = buffer;
		int_to_byte i_to_b;
		i_to_b.i = sizeof(int) + sizeof(int) + sizeof(T);

		std::copy(i_to_b.b, i_to_b.b + sizeof(int), curBuffer);
		curBuffer += sizeof(int);

		int_to_byte s_to_b;
		s_to_b.i = packet_events[typeid(T).hash_code()];

		std::copy(s_to_b.b, s_to_b.b + sizeof(int), curBuffer);
		curBuffer += sizeof(int);

		std::copy(ToByte(), ToByte() + sizeof(T), curBuffer);
		return i_to_b.i;
	}
};
#pragma pack(pop)


#define INT2 sizeof(int) + sizeof(int)

class PacketEvent
{
public:
	template <typename T, typename F>
	static void bind_event(F f)
	{
		using namespace yc_packet_event_sp;
		event[packet_events[typeid(T).hash_code()]].push_back([f](void* d, int id) { f((T*)d, id); });
	}

	template <typename T>
	static void signal_event(int id, T p)
	{
		for (auto ev : yc_packet_event_sp::event[id])
		{
			ev(p, -1);
		}
	}

	template <typename T>
	static void signal_event(int id, T p, yc::socket_t user_id)
	{
		for (auto ev : yc_packet_event_sp::event[id])
		{
			ev(p, user_id);
		}
	}
};
class yc_read_manager
{
	std::vector<char> buf;
public:

	void read(unsigned char* buf, int len, yc::socket_t id = -1)
	{
		std::vector<char> b;
		for (int i = 0; i < len; i++) 
			b.push_back(buf[i]);
		read(b, id);
	}

	void read(std::vector<char> new_packets, yc::socket_t id = -1)
	{
		buf.insert(buf.end(), new_packets.begin(), new_packets.end());
		if (buf.size() < INT2) return;
		int packet_size = *((int*)(&buf[0]));
		int packet_id = *((int*)(&buf[sizeof(int)]));
		while (buf.size() >= packet_size) {
			if (id != -1) PacketEvent::signal_event(packet_id, &buf[INT2], id);
			else		  PacketEvent::signal_event(packet_id, &buf[INT2]);
			std::vector<char> new_buffer;
			for (int i = packet_size; i < buf.size(); i++) {
				new_buffer.push_back(buf[i]);
			}
			buf = new_buffer;
			packet_size = (buf.size() >= sizeof(int)) ? (*((int*)(&buf[0]))) : packet_size;
			packet_id = (buf.size() >= INT2) ? (*((int*)(&buf[sizeof(int)]))) : packet_id;
		}
	}

};
class yc_template_packet_to_row
{
	size_t hashcode = -1;
	friend class ioev;
public:
	template<int i>
	void To()
	{
		using namespace yc_packet_event_sp;
		packet_events[hashcode] = i;
	}
};
class ioev
{
public:
	template<typename T>
	static yc_template_packet_to_row Map()
	{
		yc_template_packet_to_row p;
		p.hashcode = typeid(T).hash_code();
		return p;
	}

	template<typename T, typename F>
	static void Signal(F f)
	{
		PacketEvent::bind_event<T>(f);
	}
};

namespace yc_net
{
	template <typename T, typename F>
	void bind_ev(F f)
	{
		ioev::Signal<T>(f);
	}
}
