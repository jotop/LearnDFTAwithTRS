#include <iostream>

#include "Commutativity.h"
#include "ConnectivityTest.h"
#include "DistributivityTest.h"
#include "Tests.h"
#include "calculate_membership_queries_number_to_mock_equivalent_query.h"


void run_blind_teacher_experiment()
{
    std::cout << "==========================================================\n";
    std::cout << "Blind Teacher Experiment\n";
    std::cout << "==========================================================\n";
    std::cout << "\n";
    std::cout << "Goal: Determine how many random membership queries are needed\n";
    std::cout << "to simulate an equivalence query with a given confidence level.\n";
    std::cout << "\n";
    std::cout << "Method: For each state count, we generate pairs of non-equivalent\n";
    std::cout << "random tree automata and measure how many random trees we need to\n";
    std::cout << "test before finding a counterexample (a tree accepted by one but\n";
    std::cout << "not the other). We repeat this many times and report percentiles.\n";
    std::cout << "\n";
    std::cout << "Interpretation: If the table says '150' at 90% for 20 states,\n";
    std::cout << "it means that testing 150 random trees is enough to find a\n";
    std::cout << "counterexample in 90% of cases when the automata differ.\n";
    std::cout << "\n";
    std::cout << "Parameters:\n";

    const int MAX_TREE_DEPTH     = 100;
    const int MAX_QUERIES_BUDGET = 100000000;
    const int NUM_TRIALS         = 200;

    std::cout << "  - Max tree depth: " << MAX_TREE_DEPTH << "\n";
    std::cout << "  - Trials per state count: " << NUM_TRIALS << "\n";
    std::cout << "  - Max queries budget: " << MAX_QUERIES_BUDGET << "\n";
    std::cout << "==========================================================\n\n";

    std::vector<int> state_counts = { 5, 10, 15, 20, 25, 30, 35, 40, 45, 50 };

    auto results = mem_queries_to_eq::run_full_experiment(state_counts,
                                                          NUM_TRIALS,         // num_trials
                                                          MAX_QUERIES_BUDGET, // max_queries_budget
                                                          MAX_TREE_DEPTH      // max_tree_depth
    );

    std::cout << "\n==========================================================\n";
    std::cout << "Summary Table: Queries needed for confidence level\n";
    std::cout << "==========================================================\n";
    std::cout << std::format("{:<10} {:>10} {:>10} {:>10} {:>10}\n", "States", "70%", "80%", "90%", "99%");
    std::cout << "----------------------------------------------------------\n";

    for (const auto& r : results)
    {
        std::cout << std::format("{:<10} {:>10} {:>10} {:>10} {:>10}\n",
                                 r.num_states,
                                 r.queries_for_70_percent,
                                 r.queries_for_80_percent,
                                 r.queries_for_90_percent,
                                 r.queries_for_99_percent);
    }

    std::cout << "==========================================================\n";
}


int main()
{
    //std::cout << "Hello World!\n";
    //std::cout << "Test1\n";
    //test::test1();

    //std::cout << "\n\n\nTest2\n";
    //test::test2();
    //std::cout << "\n\n\nTest3\n";
    ////test::test3();
    //std::cout << "\n\n\nTest4\n";
    //test::test4(20);

    //std::cout << "\n\n\nTest5\n";
    //test::test5(20);

    //std::cout << "Connectivity Test\n\n";
    int k = 1000;
    while (k > 0)
    {
        test::DistributivityTest();
        //test::ConnectivityTest();
        k--;
    }

    //test::CommutativityTest();

    //test::CommutativityTestAdditionalSymbol();

    //test::DistributivityTest();

    //run_blind_teacher_experiment();
}
