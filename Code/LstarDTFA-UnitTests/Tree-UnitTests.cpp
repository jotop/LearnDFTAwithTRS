#include "../LstarDTFA/Alfabet.h"
#include "../LstarDTFA/Tree.h"
#include "../LstarDTFA/3rdParty/catch.hpp"

using alphabet::Alphabet;
using alphabet::Symbol;

SCENARIO("Building small tree and check consistency")
{
	Alphabet a = Alphabet().add_symbol(Symbol{ .m_arity = 2,.m_name = "f" }).add_symbol(Symbol{ .m_arity = 0, .m_name = "a" });

	GIVEN("Proper tree")
	{
		tree::Tree t(a);

		auto root_id = t.add_node(std::nullopt, a.get_symbol("f"));
		t.add_node(root_id, a.get_symbol("a"));
		t.add_node(root_id, a.get_symbol("a"));

		REQUIRE(t.check_consistency());
	}

	GIVEN("Incorrect tree")
	{
		tree::Tree t(a);

		auto root_id = t.add_node(std::nullopt, a.get_symbol("f"));
		t.add_node(root_id, a.get_symbol("a"));

		REQUIRE_FALSE(t.check_consistency());
	}
}