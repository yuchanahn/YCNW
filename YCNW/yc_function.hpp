#pragma once
#include <mutex>
#include <ranges>


static auto is_act_true = [](auto& i) { return i.is_active; };
static auto pis_act_true = [](auto& i) { return i->is_active; };
static auto is_act_false = [](auto& i) { return !i.is_active; };
static auto pis_act_false = [](auto& i) { return !i->is_active; };


template <typename Fs>
void invoke_all(Fs fs)
{
	for (auto& f : fs) {
		f();
	}
}

template <typename F>
void if_ture_lockfree(std::mutex& m, bool condition, F f)
{
	if (condition)
	{
		f();
	}
	else
	{
		std::lock_guard lock(m);
		f();
	}
}



auto yc_filter = [](auto f) {return std::views::filter(f); };