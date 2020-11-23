#pragma once
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>


namespace yc_net {

	template <class T>
	class thread_safe_queue
	{
	public:
		thread_safe_queue()
			: q()
			, m()
			, c()
		{}

		~thread_safe_queue()
		{}

		// Add an element to the queue.
		void enqueue(T t)
		{
			std::lock_guard<std::mutex> lock(m);
			q.push(t);
			c.notify_one();
		}

		int size()
		{
			std::lock_guard<std::mutex> lock(m);
			return static_cast<int>(q.size());
		}

		// Get the "front"-element.
		// If the queue is empty, wait till a element is avaiable.
		T dequeue()
		{
			std::unique_lock<std::mutex> lock(m);
			c.wait(lock, [&] { return !q.empty(); });
			T val = q.front();
			q.pop();
			return val;
		}

		std::vector<T> dequeue_all()
		{
			std::unique_lock<std::mutex> lock(m);
			c.wait(lock, [&] { return !q.empty(); });

			std::vector<T> v;
			while (q.size())
			{
				v.push_back(q.front());
				q.pop();
			}
			return v;
		}

	private:
		std::queue<T> q;
		mutable std::mutex m;
		std::condition_variable c;
	};


}