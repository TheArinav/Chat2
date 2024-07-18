//
// Created by ariel on 7/17/2024.
//

#ifndef CHAT2_STREAMABLESTRING_H
#define CHAT2_STREAMABLESTRING_H

#include <string>
#include <sstream>

using namespace std;

namespace classes::general{
    class StreamableString {
    public:
        // Constructors
        StreamableString() = default;
        StreamableString(const string& str) : data(str), ss(str) {}
        StreamableString(const char* str) : data(str), ss(str) {}

        // Copy constructor
        StreamableString(const StreamableString& other) : data(other.data), ss(other.data) {}

        // Move constructor
        StreamableString(StreamableString&& other) noexcept : data(std::move(other.data)), ss(other.ss.str()) {}

        // Destructor
        ~StreamableString() = default;

        // Assignment operators
        StreamableString& operator=(const std::string& str) {
            data = str;
            ss.str(data);
            ss.clear();  // Clear stream state flags
            return *this;
        }

        StreamableString& operator=(const char* str) {
            data = str;
            ss.str(data);
            ss.clear();  // Clear stream state flags
            return *this;
        }

        StreamableString& operator=(const StreamableString& other) {
            if (this != &other) {
                data = other.data;
                ss.str(data);
                ss.clear();  // Clear stream state flags
            }
            return *this;
        }

        StreamableString& operator=(StreamableString&& other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                ss.str(other.ss.str());
                ss.clear();  // Clear stream state flags
            }
            return *this;
        }

        // Conversion operator to std::string
        operator std::string() const {
            return data;
        }

        // Stream insertion operator
        template <typename T>
        StreamableString& operator<<(const T& value) {
            ss << value;
            data = ss.str();
            return *this;
        }

        // Stream extraction operator
        template <typename T>
        StreamableString& operator>>(T& value) {
            ss >> value;
            data = ss.str();
            return *this;
        }

        // String concatenation operators
        StreamableString& operator+=(const std::string& str) {
            data += str;
            ss.str(data);
            ss.clear();  // Clear stream state flags
            return *this;
        }

        StreamableString& operator+=(const char* str) {
            data += str;
            ss.str(data);
            ss.clear();  // Clear stream state flags
            return *this;
        }

        // Access element
        char& operator[](size_t pos) {
            return data[pos];
        }

        const char& operator[](size_t pos) const {
            return data[pos];
        }

        // Append functions
        StreamableString& append(const std::string& str) {
            data.append(str);
            ss.str(data);
            ss.clear();  // Clear stream state flags
            return *this;
        }

        StreamableString& append(const char* str) {
            data.append(str);
            ss.str(data);
            ss.clear();  // Clear stream state flags
            return *this;
        }

        // Other std::string member functions
        size_t length() const { return data.length(); }
        void clear() {
            data.clear();
            ss.str(data);
            ss.clear();  // Clear stream state flags
        }
        bool empty() const { return data.empty(); }
        const char* c_str() const { return data.c_str(); }

    private:
        std::string data;
        std::stringstream ss;
    };
}

#endif //CHAT2_STREAMABLESTRING_H
