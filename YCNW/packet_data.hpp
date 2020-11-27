#pragma once
#include "yc_packet.hpp"


#pragma pack(push, 1)

struct p_test_packet_t
{
	int number;
};

#pragma pack(pop)




auto packet_data_load = [] {
	ioev::Map<p_test_packet_t>().To<0>();
};