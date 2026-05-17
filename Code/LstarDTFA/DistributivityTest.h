#pragma once
#include <chrono>
#include <ctime>
#include <fstream>

#include "BottomUpDTFA.h"
#include "Lstar.h"
#include "TeacherApproximate.h"
#include "quotient_distributivity_automata.h"

namespace test {


std::optional<bottom_up_dtfta::BottomUpDTFA> random_distributivity_dfa(int states, int zero_ary_symbols)
{
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 2, .m_name = "f" });
    alph.add_symbol({ .m_arity = 1, .m_name = "g" });

    for (int i = 0; i < zero_ary_symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);
    srand(std::time(nullptr));

    std::vector<int> states_per_symbol(zero_ary_symbols);
    for (int i = 0; i < zero_ary_symbols; i++)
    {
        int q = rand() % states;
        result.add_transition({ .m_incoming_states = {},
                                .m_symbol          = std::string(1, static_cast<char>('a' + i)),
                                .m_outgoing_state  = q });
        states_per_symbol[i] = q;
    }

    std::vector<int>                          phi(states);
    std::unordered_map<int, std::vector<int>> reverse_phi;
    for (int q = 0; q < states; ++q)
    {
        phi[q] = rand() % states;
        if (!reverse_phi.contains(phi[q]))
        {
            reverse_phi[phi[q]] = {};
        }
        reverse_phi[phi[q]].emplace_back(q);
    }

    std::vector<std::vector<int>> f(states, std::vector<int>(states, -1));
    for (int q1 = 0; q1 < states; ++q1)
    {
        for (int q2 = 0; q2 < states; ++q2)
        {
            f[q1][q2] = rand() % states;
        }
    }

    int  counter           = 500;
    bool correction_needed = true;
    while (correction_needed && counter > 0)
    {
        correction_needed = false;
        counter--;
        for (int q1 = 0; q1 < states; ++q1)
        {
            for (int q2 = 0; q2 < states; ++q2)
            {
                int lhs = phi[f[q1][q2]];
                int rhs = f[phi[q1]][phi[q2]];
                if (lhs != rhs)
                {
                    correction_needed = true;
                    if (reverse_phi.contains(rhs))
                    {
                        f[q1][q2] = reverse_phi[rhs][rand() % reverse_phi[rhs].size()];
                    }
                    else
                    {
                        f[phi[q1]][phi[q2]] = phi[f[q1][q2]];
                    }
                }
            }
        }
    }

    if (correction_needed)
    {
        return std::nullopt;
    }

    for (int q1 = 0; q1 < states; ++q1)
    {
        for (int q2 = 0; q2 < states; ++q2)
        {
            result.add_transition({
                  .m_incoming_states = { q1, q2 },
                  .m_symbol          = "f",
                  .m_outgoing_state  = f[q1][q2]
            });
        }
    }

    for (int q = 0; q < states; ++q)
    {
        result.add_transition({ .m_incoming_states = { q }, .m_symbol = "g", .m_outgoing_state = phi[q] });
    }

    for (int i = 0; i < (states + 9) / 10; ++i)
    {
        int cand = rand() % states;
        result.set_final_state(cand);
    }

    result.set_states(states);
    return result;
}


void DistributivityTest()
{
    std::cout << "\n\nDistributivityTest\n";

    auto now       = std::chrono::system_clock::now();
    auto now_time  = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = {};
    localtime_s(&now_tm, &now_time);

    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S", &now_tm);

    std::string filename = std::string("DistributivityTestResults_") + time_buf + ".csv";

    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create file " << filename << "\n";
        return;
    }

    std::cout << "Saving results to: " << filename << "\n";

    file << "Number of leaf symbols,States in tree automata,"
            "Standard equivalence queries,Standard time,"
            "Advice equivalence queries,Advice time,"
            "Approximate time,Approximate tokens,Approximate is_found,"
            "Approximate advice time,Approximate advice tokens,Approximate advice is_found,"
            "Approximate random check time,Approximate random check tokens,Approximate random check is_found,"
            "Approximate counting heuristic time,Approximate counting heuristic tokens,Approximate counting heuristic is_found\n";

    int number_of_problems_automatas = 0;

    int NUM_RANDOM_TRIES = 5000000;
    int MAX_TREE_DEPTH   = 1000;

    int test_cases = 5;
    while (test_cases > 0)
    {
        int MINSYM  = 2;
        int MAXSYM  = 50;
        int symbols = rand() % (MAXSYM - MINSYM) + MINSYM;

        int MINSTATES = 40;
        int MAXSTATES = 200;
        int states    = rand() % (MAXSTATES - MINSTATES) + MINSTATES;

        auto tree_automata_opt = quotient_distributivity_automata(symbols, states, symbols);
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
        a1.add_node({}, "g");
        a1.add_node({ 0 }, "f");
        a1.add_node({ 1 }, "T1");
        a1.add_node({ 1 }, "T2");

        tree::DAGTree b1(expanded_alphabet);
        b1.add_node({}, "f");
        b1.add_node({ 0 }, "g");
        b1.add_node({ 0 }, "g");
        b1.add_node({ 1 }, "T1");
        b1.add_node({ 2 }, "T2");

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

        // 5-7. Approximate learning
        lstar::TeacherApproximate approx_teacher(tree_automata, NUM_RANDOM_TRIES, MAX_TREE_DEPTH);
        lstar::Student            approx_student(approx_teacher, tree_automata.get_alphabet());

        start              = std::chrono::high_resolution_clock::now();
        auto approx_result = approx_student.learn();
        stop               = std::chrono::high_resolution_clock::now();
        auto elapsed_approx = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        bool approx_found = approx_result.has_value() && approx_teacher.is_proper_automata_found;

        std::cout << "Approximate time: " << elapsed_approx.count() << "s "
                  << "tokens: " << approx_teacher.get_membership_tokens()
                  << " found: " << approx_found << "\n";

        // 8-10. Approximate learning with advice
        lstar::TeacherApproximate approx_teacher_advice(tree_automata, NUM_RANDOM_TRIES, MAX_TREE_DEPTH);
        lstar::StudentAdvice      approx_student_advice(approx_teacher_advice,
                                                        tree_automata.get_alphabet(),
                                                        advice);

        start                   = std::chrono::high_resolution_clock::now();
        auto approx_adv_result  = approx_student_advice.learn();
        stop                    = std::chrono::high_resolution_clock::now();
        auto elapsed_approx_adv = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        bool approx_adv_found = approx_adv_result.has_value() && approx_teacher_advice.is_proper_automata_found;

        std::cout << "Approximate advice time: " << elapsed_approx_adv.count() << "s "
                  << "tokens: " << approx_teacher_advice.get_membership_tokens()
                  << " found: " << approx_adv_found << "\n";

        // 11-13. Approximate learning with advice (RandomCheck)
        lstar::TeacherApproximate      approx_teacher_rc(tree_automata, NUM_RANDOM_TRIES, MAX_TREE_DEPTH);
        lstar::StudentAdviceRandomCheck approx_student_rc(approx_teacher_rc,
                                                          tree_automata.get_alphabet(),
                                                          advice);

        start                = std::chrono::high_resolution_clock::now();
        auto approx_rc_result = approx_student_rc.learn();
        stop                 = std::chrono::high_resolution_clock::now();
        auto elapsed_approx_rc = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        bool approx_rc_found = approx_rc_result.has_value() && approx_teacher_rc.is_proper_automata_found;

        std::cout << "Approximate random check time: " << elapsed_approx_rc.count() << "s "
                  << "tokens: " << approx_teacher_rc.get_membership_tokens()
                  << " found: " << approx_rc_found << "\n";

        // 14-16. Approximate learning with advice (CountingHeuristic)
        lstar::TeacherApproximate              approx_teacher_ch(tree_automata, NUM_RANDOM_TRIES, MAX_TREE_DEPTH);
        lstar::StudentAdviceCountingHeuristic  approx_student_ch(approx_teacher_ch,
                                                                  tree_automata.get_alphabet(),
                                                                  advice);

        start                = std::chrono::high_resolution_clock::now();
        auto approx_ch_result = approx_student_ch.learn();
        stop                 = std::chrono::high_resolution_clock::now();
        auto elapsed_approx_ch = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        bool approx_ch_found = approx_ch_result.has_value() && approx_teacher_ch.is_proper_automata_found;

        std::cout << "Approximate counting heuristic time: " << elapsed_approx_ch.count() << "s "
                  << "tokens: " << approx_teacher_ch.get_membership_tokens()
                  << " found: " << approx_ch_found << "\n";

        file << std::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n",
                            symbols,
                            tree_automata.get_states(),
                            basic_student.teacher.get_number_eq(),
                            elapsed.count(),
                            basic_student_advice.teacher.get_number_eq(),
                            elapsed_adv.count(),
                            elapsed_approx.count(),
                            approx_teacher.get_membership_tokens(),
                            approx_found ? 1 : 0,
                            elapsed_approx_adv.count(),
                            approx_teacher_advice.get_membership_tokens(),
                            approx_adv_found ? 1 : 0,
                            elapsed_approx_rc.count(),
                            approx_teacher_rc.get_membership_tokens(),
                            approx_rc_found ? 1 : 0,
                            elapsed_approx_ch.count(),
                            approx_teacher_ch.get_membership_tokens(),
                            approx_ch_found ? 1 : 0);

        test_cases--;
    }

    file.close();
    std::cout << "problem automatas " << number_of_problems_automatas << "\n";
    std::cout << "DistributivityTest Ends with Success\n";
}

} // namespace test
