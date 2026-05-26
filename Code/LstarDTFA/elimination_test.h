#pragma once
#include <chrono>
#include <ctime>
#include <fstream>
#include <numeric>

#include "BottomUpDTFA.h"
#include "Lstar.h"
#include "TeacherApproximate.h"

namespace test {


std::optional<bottom_up_dtfta::BottomUpDTFA> random_elimination_dfa(int states, int zero_ary_symbols)
{
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 2, .m_name = "f1" });
    alph.add_symbol({ .m_arity = 1, .m_name = "g1" });

    for (int i = 0; i < zero_ary_symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);

    // Generate g (phi) as a random injective function (różnowartościowa).
    // Since domain = codomain = {0..states-1}, an injection is a permutation.
    // The f-generation logic below handles the general non-surjective case:
    // only pairs (s1, s2) with both in image(phi) are constrained by the rule.
    std::vector<int> phi(states);
    std::iota(phi.begin(), phi.end(), 0);
    for (int i = states - 1; i > 0; --i)
    {
        int j = rand() % (i + 1);
        std::swap(phi[i], phi[j]);
    }

    // Determine image of phi
    std::vector<bool> in_image(states, false);
    for (int q = 0; q < states; ++q)
    {
        in_image[phi[q]] = true;
    }

    // Build inverse of phi on its image (well-defined since phi is injective)
    std::vector<int> phi_inv(states, -1);
    for (int q = 0; q < states; ++q)
    {
        phi_inv[phi[q]] = q;
    }

    // Build f so that f(phi[q1], phi[q2]) = q1 for all q1, q2.
    // For s1 in image(phi) and s2 in image(phi): f[s1][s2] = phi_inv[s1].
    // For pairs where s1 or s2 is NOT in image(phi): assign randomly.
    std::vector<std::vector<int>> f(states, std::vector<int>(states, -1));

    for (int s1 = 0; s1 < states; ++s1)
    {
        for (int s2 = 0; s2 < states; ++s2)
        {
            if (in_image[s1] && in_image[s2])
            {
                f[s1][s2] = phi_inv[s1];
            }
            else
            {
                f[s1][s2] = rand() % states;
            }
        }
    }

    // Assign zero-ary symbols to random states
    for (int i = 0; i < zero_ary_symbols; i++)
    {
        int q = rand() % states;
        result.add_transition({ .m_incoming_states = {},
                                .m_symbol          = std::string(1, static_cast<char>('a' + i)),
                                .m_outgoing_state  = q });
    }

    // Add g transitions
    for (int q = 0; q < states; ++q)
    {
        result.add_transition({ .m_incoming_states = { q }, .m_symbol = "g1", .m_outgoing_state = phi[q] });
    }

    // Add f transitions
    for (int q1 = 0; q1 < states; ++q1)
    {
        for (int q2 = 0; q2 < states; ++q2)
        {
            result.add_transition({
                  .m_incoming_states = { q1, q2 },
                  .m_symbol          = "f1",
                  .m_outgoing_state  = f[q1][q2]
            });
        }
    }

    // Set accepting states (~10% of states)
    for (int i = 0; i < (states + 9) / 10; ++i)
    {
        int cand = rand() % states;
        result.set_final_state(cand);
    }

    result.set_states(states);
    return result;
}


void EliminationTest()
{
    std::cout << "\n\nEliminationTest\n";
    srand(static_cast<unsigned>(std::time(nullptr)));

    auto    now      = std::chrono::system_clock::now();
    auto    now_time = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm   = {};
    localtime_s(&now_tm, &now_time);

    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S", &now_tm);

    std::string filename = std::string("EliminationTestResults_") + time_buf + ".csv";

    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create file " << filename << "\n";
        return;
    }

    std::cout << "Saving results to: " << filename << "\n";

    file << "Number of leaf symbols,States in tree automata,"
            "Standard equivalence queries,Standard time,"
            "Advice equivalence queries,Advice time\n";

    int number_of_problems_automatas = 0;

    int test_cases = 2;
    while (test_cases > 0)
    {
        int MINSYM  = 2;
        int MAXSYM  = 20;
        int symbols = rand() % (MAXSYM - MINSYM) + MINSYM;

        int MINSTATES = 30;
        int MAXSTATES = 100;
        int states    = rand() % (MAXSTATES - MINSTATES) + MINSTATES;

        auto tree_automata_opt = random_elimination_dfa(states, symbols);
        if (!tree_automata_opt.has_value())
        {
            continue;
        }
        auto tree_automata = tree_automata_opt.value();

        std::cout << std::format("\nTree Automata states {}\n", tree_automata.get_states());

        // 1-2. Normal learning (standard)
        lstar::Teacher basic_teacher_for_student(tree_automata);
        lstar::Student basic_student(basic_teacher_for_student, tree_automata.get_alphabet());
        auto           start = std::chrono::high_resolution_clock::now();
        if (!basic_student.learn().has_value())
        {
            number_of_problems_automatas++;
            continue;
        }
        auto stop    = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        if (basic_student.teacher.get_number_eq() < 5)
        {
            continue;
        }

        std::cout << "No Advice time: " << elapsed.count() << "s " << basic_student.teacher.get_notes();

        // 3-4. Normal learning with advice (distributivity rule)
        alphabet::Alphabet expanded_alphabet = tree_automata.get_alphabet();
        expanded_alphabet.add_symbol({ 0, "T1" });
        expanded_alphabet.add_symbol({ 0, "T2" });

        TermRewritingSystem advice;
        tree::DAGTree       a1(expanded_alphabet);
        a1.add_node({}, "f1");
        a1.add_node({ 0 }, "g1");
        a1.add_node({ 0 }, "g1");
        a1.add_node({ 1 }, "T1");
        a1.add_node({ 2 }, "T2");

        tree::DAGTree b1(expanded_alphabet);
        b1.add_node({}, "T1");

        advice.add_rule(TermRule(2, a1, b1));

        lstar::Teacher       basic_teacher_for_student_advice(tree_automata);
        lstar::StudentAdvice basic_student_advice(basic_teacher_for_student_advice,
                                                  tree_automata.get_alphabet(),
                                                  advice);

        start = std::chrono::high_resolution_clock::now();
        if (!basic_student_advice.learn().has_value())
        {
            number_of_problems_automatas++;
            continue;
        }
        stop             = std::chrono::high_resolution_clock::now();
        auto elapsed_adv = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        std::cout << "Advice time: " << elapsed_adv.count() << "s " << basic_student_advice.teacher.get_notes();

        file << std::format("{},{},{},{},{},{}\n",
                            symbols,
                            tree_automata.get_states(),
                            basic_student.teacher.get_number_eq(),
                            elapsed.count(),
                            basic_student_advice.teacher.get_number_eq(),
                            elapsed_adv.count());

        test_cases--;
    }

    file.close();
    std::cout << "problem automatas " << number_of_problems_automatas << "\n";
    std::cout << "DistributivityTest Ends with Success\n";
}

} // namespace test
