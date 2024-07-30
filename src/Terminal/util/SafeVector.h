#ifndef SAFEVECTOR_H
#define SAFEVECTOR_H

#include <vector>
#include <mutex>
#include <stdexcept>

namespace terminal::util {
    template <typename T>
    class SafeVector {
    public:
        SafeVector() = default;

        // Add elements to the vector
        void push_back(const T& value) {
            std::lock_guard<std::mutex> lock(mutex_);
            vec_.push_back(value);
        }

        // Access elements with thread safety
        T operator[](size_t index) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (index >= vec_.size()) {
                throw std::out_of_range("Index out of range");
            }
            return vec_[index];
        }

        // Access elements with thread safety (const version)
        T operator[](size_t index) const {
            std::lock_guard<std::mutex> lock(mutex_);
            if (index >= vec_.size()) {
                throw std::out_of_range("Index out of range");
            }
            return vec_[index];
        }

        // Get size of the vector
        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return vec_.size();
        }

        // Check if the vector is empty
        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return vec_.empty();
        }

        // Other necessary functions can be added similarly...

    private:
        std::vector<T> vec_;
        mutable std::mutex mutex_;
    };
}

#endif // SAFEVECTOR_H
