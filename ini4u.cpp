#include <iostream>
#include <fstream>
#include "ini.hpp"

class assertion_error {
public:
	assertion_error(const char* what) noexcept : _what { what } {}

	const char* what() const noexcept
	{ return _what; }

private:
	const char* _what;
};

static uint32_t assertions = 0;
#define ASSERT(evl, msg) if (!evl) throw assertion_error(msg); \
						 else printf("Assertion #%d passed.\n", ++assertions);
#define TEST_EQ(v1, v2) ASSERT((v1 == v2), "Test failed")
#define TEST_NEQ(v1, v2) ASSERT((v1 != v2), "Test failed")
#define TEST_LESS(v1, v2) ASSERT((v1 < v2), "Test failed")
#define TEST_MORE(v1, v2) ASSERT((v1 > v2), "Test failed")
#define TEST_LESS_OR_EQ(v1, v2) ASSERT((v1 <= v2), "Test failed")
#define TEST_MORE_OR_EQ(v1, v2) ASSERT((v1 >= v2), "Test failed")

int main()
{
	auto test_node_1 = ini::node::from_raw("    test_name       =     test_value             ");
	// string
	TEST_EQ(test_node_1.name(), "test_name");
	TEST_EQ(test_node_1.get(ini::string_deserializer{ ini::deserializer_modes::trim }), "test_value");
	// integer
	auto test_node_2 = ini::node::from_raw("name=10");
	TEST_EQ(test_node_2.get(ini::i32_deserializer{}), 10);
	// negative integer
	auto test_node_3 = ini::node::from_raw("name=-10");
	TEST_EQ(test_node_3.get(ini::i32_deserializer{}), -10);
	// 32 bit floating point
	auto test_node_4 = ini::node::from_raw("name=2.5");
	TEST_EQ(test_node_4.get(ini::f32_deserializer{}), 2.5f);
	// integer overflow
	auto test_node_5 = ini::node::from_raw("name=256");
	TEST_EQ(test_node_5.get(ini::u8_deserializer{}), 0);

	auto test_header_1 = ini::header::from_raw("   [  test_name      ]  ");
	TEST_EQ(test_header_1.name(), "test_name");

	// tests exceptions
	{
		bool threw = false;
		try {
			auto test_header_2 = ini::header::from_raw("      [     malformed                  ");
		}
		catch (ini::error::malformed_header e) {
			threw = true;
		}
		TEST_EQ(threw, true);

		threw = false;
		try {
			auto test_header_3 = ini::header::from_raw("     malformed     ]      ");
		}
		catch (ini::error::malformed_header e) {
			threw = true;
		}
		TEST_EQ(threw, true);
	}

	auto test_node_6 = ini::node("value", ini::i32_serializer{}(20));
	TEST_EQ(test_node_6.name(), "value");
	TEST_EQ(test_node_6.get(ini::i32_deserializer{}), 20);
	
    ini::structure str = ini::structure::from_raw(
            "[header1]\n"
            "this-is-a-node=value\n"
            "node1=test_value ; this is a comment. this should be ignored\n"
            "node2=test_value2\n"
            "[header2]\n"
            "morenode  =         320"
    );

    for (const auto& node : str.all_nodes_of("header2")) {
        std::cout << node.get(ini::u32_deserializer{}) << '\n';
    }


	printf("\nAll %d tests passed successfully.", assertions);
	std::endl(std::cout);
	return 0;
}
