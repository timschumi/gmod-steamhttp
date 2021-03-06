#include <deque>
#include <mutex>

template <class T>
class LockableQueue {
	std::deque<T> queue;
	std::mutex mutex;

public:
	void push(T element);
	T pop();
	size_t size();
	bool empty();
};

template <class T>
void LockableQueue<T>::push(T element) {
	std::lock_guard<std::mutex> lock(mutex);
	queue.push_back(element);
}

template <class T>
T LockableQueue<T>::pop() {
	T element;
	std::lock_guard<std::mutex> lock(mutex);

	element = queue.front();
	queue.pop_front();

	return element;
}

template <class T>
size_t LockableQueue<T>::size() {
	std::lock_guard<std::mutex> lock(mutex);
	return queue.size();
}

template <class T>
bool LockableQueue<T>::empty() {
	return this->size() == 0;
}
