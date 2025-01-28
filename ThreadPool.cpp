/*
Author: Dan Rehberg
Date Modified: January 28, 2025
Purpose: Thread pool that can switch between sleeping and spinning threads for more control in parallel workflow.
Pending: Modification made to pass data for data parallel work in the dispatch function call
			this avoids the inability to conveniently pass a non-static member function to the dispatch work function pointer.
			Getting around this could rely on Functors and Templates, but this keeps the single function call agnostic to types
				by exploiting a static cast to void*
			This is reasonable secure in preserving ownership and security as private class data could only be passed through
				the class invoking a dispatch request (getting to define which work function has permission to modify their data).
*/

#include "ThreadPool.hpp"
#include <iostream>

ThreadsPool::ThreadsPool(size_t count) : bells(wakeUp, std::defer_lock), quit(false), spin(true), threadCount(count)
{
	clock = 0;

	workPartial = 0;
	workTotal = 0;

	threadPool = new std::thread[threadCount];
	wake = new volatile size_t[threadCount + 1];
	waiting.store(0);
	for (size_t i = 0; i < threadCount; ++i)
	{
		threadPool[i] = std::move(std::thread(&ThreadsPool::g, this, i + 1));
		wake[i + 1] = 0;
	}

	while (waiting != threadCount) {}
}

ThreadsPool::~ThreadsPool()
{
	workPartial = 0;
	workTotal = 0;
	quit = true;
	waiting.store(0);
	clock = wake[1] + 1;
	sleep.notify_all();

	for (size_t i = 0; i < threadCount; ++i)
		threadPool[i].join();

	funcData = nullptr;
	funcTask = nullptr;

	delete[] threadPool;
	delete[] wake;

	threadPool = nullptr;
	wake = nullptr;
}

//[[TO DO]], 1/26/2024 -- will be including this behavior again for functions which have access to global data being processed
/*void ThreadsPool::dispatch(size_t totalWork, void(*job)(std::mutex&, size_t, size_t), bool postSpin)
{
	funcData = job;
	workTotal = totalWork;
	size_t partial = (totalWork + threadCount) / (threadCount + 1);
	workPartial = partial;
	parallelData = true;
	bool prevSpin = spin;
	spin = postSpin;
	waiting.store(0);
	++clock;
	if (!prevSpin)
	{
		//for (size_t i = 1; i < threadCount; ++i)wake[i] = true;
		//std::lock_guard<std::mutex> alarm(wakeUp);
		bells.lock();//Pessimism be damned, notify all does not otherwise guarantee dispatch of all available threads :( without acquiring the lock
		sleep.notify_all();
		bells.unlock();
		//std::cout << clock << '\n';
	}


	if (partial > totalWork) partial = totalWork;
	job(m, 0, partial);

	while (waiting != threadCount) {}
}*/

void ThreadsPool::dispatch(size_t totalWork, void(*job)(std::mutex&, size_t, size_t, void*), void* _data, bool postSpin)
{
	data = _data;
	funcData = job;
	workTotal = totalWork;
	size_t partial = (totalWork + threadCount) / (threadCount + 1);
	workPartial = partial;
	parallelData = true;
	bool prevSpin = spin;
	spin = postSpin;
	waiting.store(0);
	++clock;
	if (!prevSpin)
	{
		//for (size_t i = 1; i < threadCount; ++i)wake[i] = true;
		//std::lock_guard<std::mutex> alarm(wakeUp);
		bells.lock();//Pessimism be damned, notify all does not otherwise guarantee dispatch of all available threads :( without acquiring the lock
		sleep.notify_all();
		bells.unlock();
		//std::cout << clock << '\n';
	}


	if (partial > totalWork) partial = totalWork;
	job(m, 0, partial, _data);

	while (waiting != threadCount) {}
}

void ThreadsPool::g(const size_t id)
{
	std::unique_lock<std::mutex> alarmClock(wakeUp, std::defer_lock);

	while (!quit)
	{
		waiting.fetch_add(1);
		size_t next = wake[id] + 1;
		if (spin)
			while (next != clock) {}
		else//Note, this might need to be distinct bools for each thread
		{
			alarmClock.lock();
			sleep.wait(alarmClock, [&]() {return next == clock; });
			alarmClock.unlock();
		}

		/*while (next != clock)
		{
			if (!spin)
			{
				//std::unique_lock<std::mutex> alarmClock(wakeUp);
				alarmClock.lock();
				sleep.wait(alarmClock, [&]() {return next == clock; });
				alarmClock.unlock();
			}
		}*/
		size_t partial = workPartial, total = workTotal;
		size_t t0 = id * partial;
		size_t t1 = t0 + partial;
		if (t0 >= total)
		{
			t0 = total;
			t1 = total;
		}
		else if (t1 > total)t1 = total;

		if (parallelData)
			if (t0 < t1)funcData(m, t0, t1, data);//funcData(m, t0, t1);
		else
			for (size_t i = t0; i < t1; ++i)
				funcTask(m, i);
		wake[id] = next;
	}
}

ThreadsPool& ThreadsPool::pool()
{
	static ThreadsPool pool(15);
	return pool;
}