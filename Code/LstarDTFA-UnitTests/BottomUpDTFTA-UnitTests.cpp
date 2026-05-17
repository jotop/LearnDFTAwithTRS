#include "../LstarDTFA/BottomUpDTFA.h"
#include "../LstarDTFA/3rdParty/catch.hpp"

using bottom_up_dtfta::BottomUpDTFA;

SCENARIO("Simple automata which accept or denied tree")
{
	GIVEN("Simple automata")
	{
		auto a = alphabet::Alphabet().add_symbol({ .m_arity = 2, .m_name = "f" }) //
			.add_symbol({ .m_arity = 1, .m_name = "g" })//
			.add_symbol({ .m_arity = 0, .m_name = "a" })//
			.add_symbol({ .m_arity = 0, .m_name = "b" });//

		BottomUpDTFA dtfa = BottomUpDTFA(a, 2, { 1 })//
			.add_transition({ .m_incoming_states = {}, .m_symbol = "a", .m_outgoing_state = 1 })//
			.add_transition({ .m_incoming_states = {}, .m_symbol = "b", .m_outgoing_state = 0 })//
			.add_transition({ .m_incoming_states = {1}, .m_symbol = "g", .m_outgoing_state = 1 })//
			.add_transition({ .m_incoming_states = {0}, .m_symbol = "g", .m_outgoing_state = 0 })//
			.add_transition({ .m_incoming_states = {0,1}, .m_symbol = "f", .m_outgoing_state = 1 })//
			.add_transition({ .m_incoming_states = {1,0}, .m_symbol = "f", .m_outgoing_state = 1 })//
			.add_transition({ .m_incoming_states = {1,1}, .m_symbol = "f", .m_outgoing_state = 1 })//
			.add_transition({ .m_incoming_states = {0,0}, .m_symbol = "f", .m_outgoing_state = 0 });//

		WHEN("Small proper tree")
		{
			tree::Tree only_a(a);
			only_a.add_node(std::nullopt, a.get_symbol("a"));
			REQUIRE(dtfa.is_accepted(only_a));
		}

		WHEN("3-level proper tree")
		{
			tree::Tree t(a);
			t.add_node(std::nullopt, a.get_symbol("f")); // 0
			t.add_node(0, a.get_symbol("f")); // 1
			t.add_node(0, a.get_symbol("g")); // 2
			t.add_node(1, a.get_symbol("b")); // 3
			t.add_node(1, a.get_symbol("a")); // 4
			t.add_node(2, a.get_symbol("a")); // 5
			REQUIRE(dtfa.is_accepted(t));
		}

		WHEN("3-level not accepted tree")
		{
			tree::Tree t(a);
			t.add_node(std::nullopt, a.get_symbol("f")); // 0
			t.add_node(0, a.get_symbol("f")); // 1
			t.add_node(0, a.get_symbol("g")); // 2
			t.add_node(1, a.get_symbol("b")); // 3
			t.add_node(1, a.get_symbol("b")); // 4
			t.add_node(2, a.get_symbol("b")); // 5
			REQUIRE_FALSE(dtfa.is_accepted(t));
		}
	}
}