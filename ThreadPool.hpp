/*
Author: Dan Rehberg
Date: 12/26/2024
Purpose: Thread pool class that can be accessed and depolyed in most applications.
	This is achieved by defining the system as a singleton for general global access.
		Granted, this could pose issues with the threads accessing the pool.
	Pool offers two execution paths for best use of Data and Task parallelism.
	Pool can also have the threads switch between sleep and spinning states.
	The thread dispatching this pool can also be involved in the execution process.
Pending: Update changes described in source file.
*/

#ifndef __THREADS_POOL__
#define __THREADS_POOL__

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class ThreadsPool final
{
public:
	//Data parallel execution
	//void dispatch(size_t totalWork, void(*job)(std::mutex&, size_t, size_t), bool postSpin = false);
	//	Version 2, pass in the shared data to work over
	void dispatch(size_t totalWork, void(*job)(std::mutex&, size_t, size_t, void*), void* data = nullptr, bool postSpin = false);
	//Task parallel execution
	void dispatch(size_t totalWork, void(*job)(std::mutex&, size_t), bool postSpin = false);
	static ThreadsPool& pool();
private:
	ThreadsPool(size_t count);
	~ThreadsPool();
	ThreadsPool(const ThreadsPool&) = delete;
	ThreadsPool& operator=(const ThreadsPool&) = delete;
	std::unique_lock<std::mutex> bells;
	volatile size_t clock;
	void* volatile data = nullptr;
	//void(* volatile funcData)(std::mutex&, size_t, size_t) = nullptr;
	void(* volatile funcData)(std::mutex&, size_t, size_t, void*) = nullptr;
	void(*funcTask)(std::mutex&, size_t) = nullptr;
	void g(const std::size_t id);
	std::mutex m;
	volatile bool parallelData;
	volatile bool quit;
	std::condition_variable sleep;
	volatile bool spin;
	const size_t threadCount;
	std::thread* threadPool;
	volatile size_t* wake;
	std::mutex wakeUp;
	volatile std::atomic_uint16_t waiting;
	volatile size_t workPartial;
	volatile size_t workTotal;
};

#endif