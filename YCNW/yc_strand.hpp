#pragma once
#include <thread>
#include <vector>
#include <ranges>
#include <mutex>
#include <functional>

#include "yc_function.hpp"
#include "yc_thread_safe_queue.hpp"


namespace yc_net {
	struct strand
	{
		int id;
		std::thread::id cur_th_id;
		bool is_active;

		thread_safe_queue<std::function<void()>> worker;
	};

	// 좀있다가 수정 예정....
	// ㅇㅋ 일단 커밋하자.
	struct strand_pool
	{
		std::vector<strand> strands;
		std::mutex m;

		strand& new_strand() {
			std::lock_guard<std::mutex> lock(m);

			for (auto& i : strands | std::views::filter(is_not_act_true)
								   | std::views::take(1)) {
				return i;
			}

			strands.push_back(strand{ static_cast<int>(strands.size()), std::this_thread::get_id(), true });

			return std::reference_wrapper<strand>(strands.back());
		}

		void run_in_this_thread(bool& stop_button)
		{
			auto t_id = std::this_thread::get_id();
			while (!stop_button) {
				for (auto& i : strands  | std::views::filter(is_act_true)
										| std::views::filter([t_id](auto& i) { return i.cur_th_id == t_id; }))
				{
					while (i.worker.size()) {
						auto fs = i.worker.dequeue_all();
						i.is_active = false;

						for (auto& f : i.worker.dequeue_all()) {
							f();
						}
					}
				}
			}
		}
	};


}