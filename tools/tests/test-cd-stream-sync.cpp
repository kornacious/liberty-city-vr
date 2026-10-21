#include "../../overlay/src/core/QuestCdStream.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <cstring>

using queststream::Queue;
using namespace std::chrono_literals;

static void Check(bool value, const char *message)
{
	if(!value){ std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct Reader {
	std::mutex mutex;
	std::condition_variable cv;
	bool hold = false;
	int entered = 0;
	static bool Read(int fd, void *buffer, std::size_t bytes, std::uint64_t offset, void *context)
	{
		auto &self = *static_cast<Reader *>(context);
		std::unique_lock<std::mutex> lock(self.mutex);
		++self.entered;
		self.cv.notify_all();
		self.cv.wait(lock, [&] { return !self.hold; });
		std::memset(buffer, static_cast<int>(offset & 255), bytes);
		return fd >= 0;
	}
	void Hold() { std::lock_guard<std::mutex> lock(mutex); hold = true; }
	void Release() { std::lock_guard<std::mutex> lock(mutex); hold = false; cv.notify_all(); }
	void WaitFor(int count)
	{
		std::unique_lock<std::mutex> lock(mutex);
		Check(cv.wait_for(lock, 5s, [&] { return entered >= count; }), "worker starts request");
	}
};

int main()
{
	// Give a lost wakeup a finite test failure instead of hanging a test run.
	std::promise<void> finished;
	auto exitSignal = finished.get_future();
	std::thread deadline([&] {
		if(exitSignal.wait_for(30s) != std::future_status::ready){
			std::fprintf(stderr, "FAIL: streaming synchronization deadlocked\n");
			std::_Exit(2);
		}
	});
	Reader reader;
	Queue queue;
	queue.Start(2, Reader::Read, &reader);
	char a[64]{}, b[64]{};
	reader.Hold();
	Check(queue.Submit(0, 1, a, 17, sizeof(a)) == Queue::Accepted, "submit read");
	reader.WaitFor(1);
	Check(queue.Status(0) == Queue::Reading, "active read status");
	Check(queue.Submit(0, 1, a, 17, sizeof(a)) == Queue::Accepted, "duplicate active read");
	queue.Submit(1, 1, b, 29, sizeof(b));
	Check(queue.Status(1) == Queue::Waiting, "second channel queued");
	auto waiter = std::async(std::launch::async, [&] { return queue.Sync(0); });
	Check(waiter.wait_for(10ms) == std::future_status::timeout, "sync waits for buffer ownership");
	reader.Release();
	Check(waiter.get() == Queue::Idle, "completion wakes waiter");
	Check(queue.Sync(1) == Queue::Idle && a[63] == 17 && b[63] == 29, "both buffers complete");

	// Completion before waiter registration must also be remembered. Polling
	// Status to Idle guarantees completion without consuming a wakeup token.
	for(int i = 0; i < 20000; ++i){
		queue.Submit(i & 1, 1, a, static_cast<std::uint64_t>(i), sizeof(a));
		if((i & 3) == 0){
			while(queue.Status(i & 1) != Queue::Idle) std::this_thread::yield();
		}
		Check(queue.Sync(i & 1) == Queue::Idle, "completion before/during wait");
		Check(static_cast<unsigned char>(a[63]) == (i & 255), "completion publishes buffer writes");
	}

	queue.Submit(0, -1, a, 0, sizeof(a));
	Check(queue.Sync(0) == Queue::Error, "read error reaches Sync");
	Check(queue.Status(0) == Queue::Error && queue.Status(0) == Queue::Idle, "status consumes error once");
	queue.Submit(0, -1, a, 0, sizeof(a));
	Check(queue.Sync(0, true) == Queue::Idle, "flush discards error");

	reader.Hold();
	const int next = reader.entered + 1;
	queue.Submit(0, 1, a, 45, sizeof(a));
	reader.WaitFor(next);
	auto replacement = std::async(std::launch::async, [&] { return queue.Submit(0, 1, a, 73, sizeof(a)); });
	Check(replacement.wait_for(10ms) == std::future_status::timeout, "replacement waits for old buffer writer");
	reader.Release();
	Check(replacement.get() == Queue::Accepted, "replacement accepted after old read");
	queue.Sync(0);
	Check(a[63] == 73, "old read cannot overwrite replacement");

	// Both channels outstanding, queue wrap/reuse and repeated restart/shutdown.
	for(int cycle = 0; cycle < 100; ++cycle){
		queue.Submit(0, 1, a, 11, sizeof(a));
		queue.Submit(1, 1, b, 23, sizeof(b));
		queue.Stop();
		Check(a[63] == 11 && b[63] == 23, "shutdown drains requests");
		queue.Start(2, Reader::Read, &reader);
	}
	queue.Stop();
	finished.set_value();
	deadline.join();
	std::puts("PASS: 20,000 completion races; queued reads, buffer ownership, errors and 100 restarts");
}
