#pragma once
#include <thread>
#include <vector>
#include <ranges>
#include <mutex>
#include <functional>
#include <unordered_map>

#include "yc_function.hpp"
#include "yc_thread_safe_queue.hpp"
#include "yc_net.hpp"


namespace yc_net 
{
	struct worker_info_t {
		std::thread::id thread_id;
		bool on_duty = false;
		bool is_active = true;
		std::mutex m;
	};

	std::unordered_map<worker_info_t*, std::vector<std::function<void()>>> workers;
	std::unordered_map<std::thread::id, std::vector<worker_info_t*>> worker_info_thread_mem;

	worker_info_t* create_worker()
	{
		auto& worker_info = worker_info_thread_mem[std::this_thread::get_id()];
		worker_info_t* w = nullptr;

		for (auto& i : worker_info | std::views::filter(pis_act_false))
		{
			i->is_active = true;
			w = i;
		}
		if (!w)
		{
			worker_info.push_back(new worker_info_t());
			w = worker_info.back();
		}
		return w;
	}

	auto add_sync_worker = [](worker_info_t* w, auto f) {
		std::lock_guard lock(w->m);
		workers[w].push_back(f);
	};

	void run_wokers_in_this_thread(bool& stop_button)
	{
		while (!stop_button)
		{
			for (auto& i : workers | std::views::filter([](auto& i) { return pis_act_true(i.first); })
						   		   | std::views::filter([](auto& i) { return i.second.size(); })
						   		   | std::views::filter([](auto& i) { return !(i.first->on_duty); }))
			{
				std::unique_lock lock(i.first->m);
				if (i.first->on_duty)
				{
					continue;
				}
				i.first->on_duty = true;
				auto v = i.second;
				i.second.clear();
				lock.unlock();
				invoke_all(v);
				lock.lock();
				i.first->on_duty = false;
			}
		}
	}
}