#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// One worker, as in the POSIX backend. Every queue/channel transition and
// completion predicate uses the same mutex. Completion before Sync starts is
// remembered by pending=false; completion during Sync cannot miss its waiter.
namespace queststream {
class Queue {
public:
	enum { Idle = 0, Accepted = 1, Reading = 0xff, Error = 0xfe, Waiting = 0xfa };
	using Reader = bool (*)(int, void *, std::size_t, std::uint64_t, void *);
	Queue() = default;
	Queue(const Queue &) = delete;
	Queue &operator=(const Queue &) = delete;
	~Queue() { Stop(); }

	void Start(int count, Reader reader, void *context = nullptr)
	{
		Stop();
		channels.resize(count);
		read = reader;
		readContext = context;
		stopping = false;
		worker = std::thread(&Queue::Run, this);
	}

	void Stop()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			stopping = true;
		}
		work.notify_one();
		if(worker.joinable()) worker.join();
		// The worker drains all requests before exiting, so buffers and image
		// descriptors can safely be released after Stop returns.
		channels.clear();
		requests.clear();
	}

	int Submit(int index, int fd, void *buffer, std::uint64_t offset, std::size_t bytes)
	{
		std::unique_lock<std::mutex> lock(mutex);
		auto &channel = channels.at(index);
		if(channel.pending && channel.fd == fd && channel.offset == offset &&
		   channel.bytes >= bytes && channel.buffer == buffer)
			return Accepted;
		// Replacement/flush must not let an old read keep writing into a
		// buffer after the caller reuses it. No asynchronous signal cancellation.
		done.wait(lock, [&] { return !channel.pending; });
		if(stopping) return Error;
		channel.fd = fd;
		channel.buffer = buffer;
		channel.offset = offset;
		channel.bytes = bytes;
		channel.status = Idle;
		channel.pending = true;
		requests.push_back(index);
		work.notify_one();
		return Accepted;
	}

	int Status(int index)
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto &channel = channels.at(index);
		if(channel.pending) return channel.reading ? Reading : Waiting;
		const int status = channel.status;
		channel.status = Idle;
		return status;
	}

	int Sync(int index, bool discard = false)
	{
		std::unique_lock<std::mutex> lock(mutex);
		auto &channel = channels.at(index);
		done.wait(lock, [&] { return !channel.pending; });
		if(discard) channel.status = Idle;
		return channel.status;
	}

private:
	struct Channel {
		int fd = -1;
		void *buffer = nullptr;
		std::uint64_t offset = 0;
		std::size_t bytes = 0;
		int status = Idle;
		bool pending = false;
		bool reading = false;
	};
	void Run()
	{
		std::unique_lock<std::mutex> lock(mutex);
		for(;;){
			work.wait(lock, [&] { return stopping || !requests.empty(); });
			if(requests.empty()) return;
			const int index = requests.front();
			requests.pop_front();
			auto &channel = channels[index];
			channel.reading = true;
			const Channel request = channel;
			lock.unlock();
			const bool success = read(request.fd, request.buffer, request.bytes,
			                          request.offset, readContext);
			lock.lock();
			channel.status = success ? Idle : Error;
			channel.reading = false;
			channel.pending = false;
			done.notify_all();
		}
	}
	std::mutex mutex;
	std::condition_variable work, done;
	std::vector<Channel> channels;
	std::deque<int> requests;
	std::thread worker;
	Reader read = nullptr;
	void *readContext = nullptr;
	bool stopping = true;
};
} // namespace queststream
