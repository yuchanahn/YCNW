#pragma once
#include "yc_packet.hpp"


#pragma pack(push, 1)

struct p_test_packet_t
{
	int number;
};

struct p_packet01_t
{
	int number;
};

struct p_packet02_t
{
	int number;
};


#pragma pack(pop)




auto packet_data_load = [] {
	ioev::Map<p_test_packet_t>().To<0>();
	ioev::Map<p_packet01_t>().To<1>();
	ioev::Map<p_packet02_t>().To<2>();
};