#pragma once
#include <chrono>

using namespace std::chrono;
namespace yc {
	class time
	{
		system_clock::time_point* lastT = nullptr;
		system_clock::time_point t;
		float deltaTime = 0;
	public:
		

		static time& Instance() {
			static time t;
			return t;
		}
		time() {}
		~time() {}

		static void update_delta_time() {
			if (Instance().lastT == nullptr)
			{
				Instance().lastT = new std::chrono::system_clock::time_point;
				return;
			}

			Instance().deltaTime = ((std::chrono::duration<float>)(std::chrono::system_clock::now() - *Instance().lastT)).count();
			*Instance().lastT = std::chrono::system_clock::now();
		}

		static float get_delta_time() {
			return Instance().deltaTime;
		}

		void TimerStart() {
			t = std::chrono::system_clock::now();
		}
		float TimerEnd()
		{
			return ((std::chrono::duration<float>)(std::chrono::system_clock::now() - t)).count();
		}
	};
}