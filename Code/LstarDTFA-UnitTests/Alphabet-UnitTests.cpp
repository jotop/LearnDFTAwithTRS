#include "../LstarDTFA/Alfabet.h"
#include "../LstarDTFA/3rdParty/catch.hpp"


SCENARIO("Adding Symbols to Alphabet")
{
	GIVEN("Empty Alphabet")
	{
		alphabet::Alphabet a = alphabet::Alphabet();

		WHEN("Adding one symbol")
		{
			a.add_symbol("f,2");
			REQUIRE(a.arity("f") == 2);
		}

		WHEN("Adding three symbol")
		{
			a.set_alphabet("(f,2), (g,3), (a,0), (b,0)");
			REQUIRE(a.arity("f") == 2);
			REQUIRE(a.arity("g") == 3);
			REQUIRE(a.arity("a") == 0);
			REQUIRE(a.arity("b") == 0);
		}


	}
}

