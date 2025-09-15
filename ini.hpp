#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <sstream>
#include <concepts>
#include <format>
#include <bitset>
#include <unordered_map>

namespace ini {
    class header;
}

namespace std {

template<>
class hash<ini::header> {
public:
    size_t operator()(const ini::header& h) const noexcept;
};

}

namespace ini {

namespace parser {

[[nodiscard]]
inline std::string trim(const std::string& source) {
	std::string result(source);
	result.erase(0, result.find_first_not_of(" \n\r\t"));
	result.erase(result.find_last_not_of(" \n\r\t") + 1);
	return result;
}

} // namespace parser

namespace error {

class empty_node_value {};
class malformed_node   {};
class malformed_header {};
class cast_not_allowed {};

} // namespace error

template <typename T>
class serializer {
public:
	using type = T;

	virtual std::string operator()(const T& v) const = 0;
};

//
// TODO;
// rethink this 'modes' thing. right now it doesnt make much sense -- the way its implemented.
// the bitmask system isn't event implemented the way it's supposed to be.
//
using deserializer_mode = int;
namespace deserializer_modes {

enum {
	none,
	trim
};

}

template <typename T>
class deserializer {
public:
	static constexpr bool allow_cast = false;

	virtual const T operator()(const std::string& v) const = 0;
protected:
	deserializer(deserializer_mode mode = deserializer_modes::none) noexcept : _mode { mode } {}
	deserializer_mode _mode;

    // static?
    constexpr bool has_mode(deserializer_mode mask, deserializer_mode mode) const noexcept {
        return ((mask & mode) == mode);
    }

	// convenient function to execute simple "default" parsing onto a desired string
	std::string default_parse(const std::string& v) const noexcept {
        if (has_mode(_mode, deserializer_modes::trim))
            return parser::trim(v);
		return v;
	}
};

template <typename T>
class number_serializer : public serializer<T> {
public:
	number_serializer() noexcept = default;
	virtual std::string operator()(const T& v) const override {
		return std::to_string(v);
	}
};

class string_deserializer : public deserializer<std::string> {
public:
	static constexpr bool allow_cast = true;

	string_deserializer(deserializer_mode mode = deserializer_modes::none) noexcept : deserializer(mode) {}

	virtual const std::string operator()(const std::string& v) const override {
		return default_parse(v);
	}
};

using u8_serializer  = number_serializer<unsigned char>;
using u16_serializer = number_serializer<unsigned short>;
using u32_serializer = number_serializer<unsigned int>;
using u64_serializer = number_serializer<unsigned long long>;

using i8_serializer  = number_serializer<char>;
using i16_serializer = number_serializer<short>;
using i32_serializer = number_serializer<int>;
using i64_serializer = number_serializer<long long>;

using f32_serializer = number_serializer<float>;
using f64_serializer = number_serializer<double>;

template <typename T>
class number_deserializer : public deserializer<T> {
public:
	using type = T;
	static constexpr bool allow_cast = false;

	number_deserializer(deserializer_mode mode = deserializer_modes::none) noexcept : deserializer<T>(mode) {}

	virtual const T operator()(const std::string& v) const override {
		std::string buf = deserializer<T>::default_parse(v);

		if constexpr (std::is_integral<T>::value) {
			if constexpr (std::is_signed<T>::value) {
				if constexpr (sizeof(T) < 4) {
					return static_cast<T&&>(std::stoi(buf.c_str()));
				}
				else if constexpr (sizeof(T) == 4) {
					return std::stoi(buf.c_str());
				}
				else if constexpr (sizeof(T) == 8) {
					return std::stoll(buf.c_str());
				}
			}
			else {
				return static_cast<T&&>(std::stoull(buf.c_str()));
			}
		}
		else if constexpr (std::is_floating_point<T>::value) {
			if constexpr (sizeof(T) == 4) {
				return std::stof(buf.c_str());
			}
			else if constexpr (sizeof(T) == 8) {
				return std::stod(buf.c_str());
			}
		}
	}
};

using u8_deserializer  = number_deserializer<unsigned char>;
using u16_deserializer = number_deserializer<unsigned short>;
using u32_deserializer = number_deserializer<unsigned int>;
using u64_deserializer = number_deserializer<unsigned long long>;

using i8_deserializer  = number_deserializer<signed char>;
using i16_deserializer = number_deserializer<short>;
using i32_deserializer = number_deserializer<int>;
using i64_deserializer = number_deserializer<long long>;

using f32_deserializer = number_deserializer<float>;
using f64_deserializer = number_deserializer<double>;

class node {
public:
	node(const std::string& name, const std::string& value) {
		_name  = name;
		_value = value;
	}
	
	static node from_raw(const std::string& raw) {
		auto token_idx = raw.find('=');
		if (token_idx == std::string::npos)
			throw error::malformed_node();
		auto name  = parser::trim(raw.substr(0, token_idx));
		auto value = raw.substr(token_idx + 1);
		return node(name, value);
	}
	~node() = default;

	node& set_name(const std::string& v) noexcept {
		_name = v;
		return *this;
	}

	const std::string& name() const noexcept
	{ return _name; }

	const std::string& raw_value() const noexcept
	{ return _value; }

	template <typename T>
	T get(const deserializer<T>& d) {
		if (parser::trim(_value).empty())
			throw error::empty_node_value();
		return static_cast<T>(d(_value));
	}

	template <typename T>
	const T get(const deserializer<T>& d) const {
		if (parser::trim(_value).empty())
			throw error::empty_node_value();
		return static_cast<T>(d(_value));
	}

	template <typename T, class D = deserializer<T>>
	T get() {
		static_assert(D::allow_cast || std::is_same<T, typename D::type>::value, "Deserializer doesn't allow static type casting.");
		return get<T>(D{});
	}

	template <typename T, class D = deserializer<T>>
	const T get() const {
		static_assert(D::allow_cast || std::is_same<T, typename D::type>::value, "Deserializer doesn't allow static type casting.");
		return get<T>(D{});
	}

	operator std::string() const noexcept {
		return std::format("{} = {}", _name, _value);
	}

	template <typename T> requires (!std::is_same<const typename std::remove_cv<T>::type&, const node&>::value)
	node& operator =(T&& v) {
		_value = serializer<T>{}(v);
		return *this;
	}

private:
	std::string _name, _value;
};

class header {
public:
	header(const std::string& name) noexcept {
		_name = name;
	}

	static header from_raw(const std::string& raw) {
		std::string buf = parser::trim(raw);
		// finds opening and closing brackets
		auto op_b = buf.find('['), cl_b = buf.find(']');
		if ((op_b == std::string::npos) || (cl_b == std::string::npos))
			throw error::malformed_header();

		auto name = parser::trim(buf.substr(op_b + 1, cl_b - 1));
		return header(name);
	}

	header& set_name(const std::string& v) noexcept {
		_name = v;
		return *this;
	}

	const std::string name() const noexcept {
		return _name;
	}

	operator std::string() const noexcept {
		return std::format("[{}]", _name);
	}

	header& operator =(const std::string& v) noexcept {
		return set_name(v);
	}

	header& operator =(std::string&& v) noexcept {
		_name = v;
		return *this;
	}

    bool operator==(const header& rhs) const noexcept {
        return _name == rhs.name();
    }

private:
	std::string _name;
};

class structure {
public:
	structure() {
         
	}

    // parses raw data from a file into a new structure
    static structure from_raw(std::string_view data) {
        structure s;
        std::string buf = std::string(data);
        
        std::vector<std::string> lines;
        size_t last_pos = 0;

        do {
            // splits the raw content into lines
            last_pos = buf.find('\n');
            lines.push_back(buf.substr(0, last_pos));
            buf = buf.substr(last_pos + 1, buf.size());
        } while (last_pos != std::string::npos);

        // actual parsing
        {
            header current_header = header(std::string());
            for (auto& line : lines) {
                // ignores comments
                size_t comment_idx = line.find(';');
                if (comment_idx != std::string::npos) {
                    line = line.substr(0, comment_idx);
                }

                line = parser::trim(line);
                if (line.empty()) { // nothing to do
                    continue;
                }

                // header
                if (line[0] == '[') {
                    current_header = header::from_raw(line); 
                    continue;
                }

                // node
                s.add_node(current_header, node::from_raw(line));
            }
        }
        
        return s;
    }

    structure& add_node(const header& h, const node& n) {
        _tree[h.name()].push_back(n);
        return *this;
    }

    structure& add_node(const header& h, node&& n) {
        _tree[h.name()].emplace_back(n); // std::move?
        return *this;
    }

    template <typename It>
    structure& add_nodes(const header& h, It begin, It end) {
        auto& nodes = _tree[h.name()];
        std::for_each(nodes.begin(), nodes.end(), [&nodes, begin, end](const auto& v) {
            nodes.push_back(v);
        });
        return *this;
    }

    std::vector<node>& all_nodes_of(const std::string& header_name) {
        return _tree.at(header_name);
    }

private:
    std::unordered_map<header, std::vector<node>> _tree;
};

}

inline size_t std::hash<ini::header>::operator()(const ini::header& h) const noexcept {
    return std::hash<std::string>{}(h.name());
}
