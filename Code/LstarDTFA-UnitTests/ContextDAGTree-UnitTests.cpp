#include "../LstarDTFA/ContextDAGTree.h"
#include "../LstarDTFA/3rdParty/catch.hpp"


SCENARIO("Creating Contexts from DAGTree")
{
	alphabet::Alphabet A;
	A.add_symbol({ .m_arity = 2, .m_name = "f" });
	A.add_symbol({ .m_arity = 0, .m_name = "a" });
	A.add_symbol({ .m_arity = 0, .m_name = "b" });

	GIVEN("Tree f(b,b)")
	{
		tree::DAGTree t(A);
		t.add_node({}, "f");
		t.add_node({ 0 }, "b");

		tree::DAGTree temp(A);
		temp.add_node({}, "b");
		t.join_tree({ 0 }, temp);

		REQUIRE(t.nodes.size() == 2);
		REQUIRE(t.nodes[0].symbol.m_name == "f");
		REQUIRE(t.nodes[0].father.empty());
		REQUIRE(t.nodes[0].childs == std::vector<int>{1,1});
		REQUIRE(t.nodes[1].father == std::vector<int>{0});
		REQUIRE(t.nodes[1].childs.empty());
		REQUIRE(t.nodes[1].symbol.m_name == "b");

		auto res = tree::ContextDAGTree::make_context(t, 1);

		REQUIRE(res.size() == 2);



	}
}