#pragma once
#include <chrono>
#include <ctime>
#include <fstream>
#include <queue>
#include <vector>

#include "BottomUpDTFA.h"
#include "Lstar.h"
#include "TeacherApproximate.h"


namespace test {


struct DFAwords
{
    int                           states;
    int                           symbols;
    std::vector<std::vector<int>> delta;
    std::vector<int>              accepting_states;
};


DFAwords get_random_dfa(int number_of_symbols, int number_of_states)
{
    DFAwords result;
    srand(std::time(nullptr));

    result.states  = number_of_states;
    result.symbols = number_of_symbols;

    for (int s = 0; s < number_of_symbols; s++)
    {
        result.delta.emplace_back();
        for (int t = 0; t < number_of_states; t++)
        {
            result.delta[s].emplace_back(rand() % number_of_states);
        }
    }

    std::vector<bool> achievable(number_of_states, false);
    std::queue<int>   to_process;
    to_process.push(0);

    while (!to_process.empty())
    {
        auto curr = to_process.front();
        to_process.pop();

        if (achievable[curr])
        {
            continue;
        }

        achievable[curr] = true;

        for (int i = 0; i < number_of_symbols; i++)
        {
            auto next = result.delta[i][curr];

            if (!achievable[next])
            {
                to_process.push(next);
            }
        }
    }

    std::vector<int> states_to_delete;
    for (int i = 0; i < number_of_states; i++)
    {
        if (!achievable[i])
        {
            states_to_delete.emplace_back(i);
        }
    }

    for (int i = 0; i < states_to_delete.size(); i++)
    {
        auto state = states_to_delete[i] - i;

        for (int sym = 0; sym < number_of_symbols; sym++)
        {
            for (int& t : result.delta[sym])
            {
                t = t >= state ? t - 1 : t;
            }

            result.delta[sym].erase(std::next(result.delta[sym].begin(), state));
        }
    }

    result.states = result.delta[0].size();

    for (int i = 0; i < (result.states + 9) / 10; i++)
    {
        int state = rand() % result.states;

        bool contains = false;
        for (auto s : result.accepting_states)
        {
            if (state == s)
            {
                contains = true;
                break;
            }
        }

        if (!contains)
        {
            result.accepting_states.emplace_back(state);
        }
    }

    return result;
}


std::optional<bottom_up_dtfta::BottomUpDTFA> build_tree_automata(DFAwords& dfa, int max_states)
{
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 2, .m_name = "foo" });
    for (int i = 0; i < dfa.symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);

    std::queue<std::vector<int>> q;
    int                          tree_states = 0;

    std::unordered_map<std::vector<int>, int> f_to_tstate;

    for (int i = 0; i < dfa.symbols; i++)
    {
        std::vector<int> f = dfa.delta[i];
        if (!f_to_tstate.contains(f))
        {
            q.push(f);
            f_to_tstate[f] = tree_states;
            tree_states++;
        }
        result.add_transition(bottom_up_dtfta::Transition { .m_incoming_states = {},
                                                            .m_symbol = std::string(1, static_cast<char>('a' + i)),
                                                            .m_outgoing_state = f_to_tstate[f] });
    }

    auto fold = [](const std::vector<int>& v, const std::vector<int>& u)
    {
        std::vector<int> result(v.size(), 0);

        for (int i = 0; i < result.size(); i++)
        {
            result[i] = u[v[i]];
        }

        return result;
    };

    while (!q.empty())
    {
        auto front = q.front();
        q.pop();

        int front_id = f_to_tstate[front];

        for (int i = 0; i < dfa.symbols; i++)
        {
            auto new_state = fold(front, dfa.delta[i]);

            if (!f_to_tstate.contains(new_state))
            {
                q.push(new_state);
                f_to_tstate[new_state] = tree_states;
                tree_states++;

                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { front_id, f_to_tstate[dfa.delta[i]] },
                      .m_symbol          = "foo",
                      .m_outgoing_state  = tree_states - 1
                });

                continue;
            }

            bool transition_exists = false;
            auto new_state_idx     = f_to_tstate[new_state];

            for (auto& t : result.get_delta_function())
            {
                if (t.m_outgoing_state == new_state_idx && t.m_symbol == "foo" && t.m_incoming_states[0] == front_id
                    && t.m_incoming_states[1] == f_to_tstate[dfa.delta[i]])
                {
                    transition_exists = true;
                    break;
                }
            }

            if (!transition_exists)
            {
                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { front_id, f_to_tstate[dfa.delta[i]] },
                      .m_symbol          = "foo",
                      .m_outgoing_state  = new_state_idx
                });
            }
        }

        if (f_to_tstate.size() > max_states)
        {
            return std::nullopt;
        }
    }

    result.set_states(tree_states);
    for (const auto& state : f_to_tstate)
    {
        bool is_accepting = std::ranges::any_of(dfa.accepting_states, [&state](int s) { return s == state.first[0]; });
        if (is_accepting)
        {
            result.set_final_state(state.second);
        }
    }

    std::unordered_map<int, std::vector<int>> state_to_map;
    for (auto& x : f_to_tstate)
    {
        state_to_map[x.second] = x.first;
    }

    for (int i = 0; i < tree_states; i++)
    {
        for (int j = 0; j < tree_states; j++)
        {
            bool transition_exists = false;
            for (auto& t : result.get_delta_function())
            {
                if (t.m_symbol == "foo" && t.m_incoming_states[0] == i && t.m_incoming_states[1] == j)
                {
                    transition_exists = true;
                    break;
                }
            }

            if (transition_exists)
            {
                continue;
            }

            result.add_transition(bottom_up_dtfta::Transition {
                  .m_incoming_states = { i, j },
                  .m_symbol          = "foo",
                  .m_outgoing_state  = f_to_tstate[fold(state_to_map[i], state_to_map[j])]
            });
        }
    }

    return result;
}


void ConnectivityTest()
{
    std::cout << "ConnectivityTest\n";


    auto now       = std::chrono::system_clock::now();
    auto now_time  = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = {};
    localtime_s(&now_tm, &now_time);

    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H-%M-%S", &now_tm);

    std::string filename = std::string("ConnectivityTestResults_") + time_buf + ".csv";

    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create file " << filename << "\n";
        return;
    }

    std::cout << "Saving results to: " << filename << "\n";

    file << "Number of leaf symbols,States in word automata,States in tree automata,"
            "Standard equivalence queries,Standard time,"
            "Advice equivalence queries,Advice time,"
            "Approximate time,Approximate tokens,Approximate is_found,"
            "Approximate advice time,Approximate advice tokens,Approximate advice is_found,"
            "Approximate random check time,Approximate random check tokens,Approximate random check is_found,"
            "Approximate counting heuristic time,Approximate counting heuristic tokens,Approximate counting heuristic is_found\n";
    int number_of_inproper_automata = 0;

    int NUM_RANDOM_TRIES = 5000000;
    int MAX_TREE_DEPTH   = 1000;

    int test_cases = 5;
    while (test_cases > 0)
    {
        int symbols = 2 + rand() % 3;
        int states  = 2 + rand() % 15;

        auto dfa = get_random_dfa(symbols, states);

        int MAX_STATES = 200;
        int MIN_STATES = 20;

        auto tree_automata_cand = build_tree_automata(dfa, MAX_STATES);

        if (!tree_automata_cand.has_value())
        {
            continue;
        }

        auto tree_automata = tree_automata_cand.value();

        if (tree_automata.get_states() > MAX_STATES || tree_automata.get_states() < MIN_STATES)
        {
            continue;
        }

        std::cout << std::format("\nAutomata states / Tree Automata states {}/{}\n",
                                 dfa.states,
                                 tree_automata.get_states());

        // 1-2. Normal learning
        lstar::Teacher basic_teacher_for_student(tree_automata);
        lstar::Student basic_student(basic_teacher_for_student, tree_automata.get_alphabet());
        auto           start = std::chrono::high_resolution_clock::now();
        if (!basic_student.learn().has_value())
        {
            number_of_inproper_automata++;
            continue;
        }
        auto stop    = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

        std::cout << "No Advice time: " << elapsed.count() << "s " << basic_student.teacher.get_notes();

        // 3-4. Normal learning with advice
        alphabet::Alphabet expanded_alphabet = tree_automata.get_alphabet();
        expanded_alphabet.add_symbol({ 0, "T1" });
        expanded_alphabet.add_symbol({ 0, "T2" });
        expanded_alphabet.add_symbol({ 0, "T3" });

        TermRewritingSystem advice;
        tree::DAGTree       a1(expanded_alphabet);
        a1.add_node({}, "foo");
        a1.add_node({ 0 }, "T1");
        a1.add_node({ 0 }, "foo");
        a1.add_node({ 2 }, "T2");
        a1.add_node({ 2 }, "T3");

        tree::DAGTree b1(expanded_alphabet);
        b1.add_node({}, "foo");
        b1.add_node({ 0 }, "foo");
        b1.add_node({ 1 }, "T1");
        b1.add_node({ 1 }, "T2");
        b1.add_node({ 0 }, "T3");

        advice.add_rule(TermRule(3, a1, b1));

        lstar::Teacher       basic_teacher_for_student_advice(tree_automata);
        lstar::StudentAdvice basic_student_advice(basic_teacher_for_student_advice,
                                                  tree_automata.get_alphabet(),
                                                  advice);

        start = std::chrono::high_resolution_clock::now();
        if (!basic_student_advice.learn().has_value())
        {
            number_of_inproper_automata++;
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

        // 14-16. Approximate learning with advice (RandomCheck)
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

        // 17-19. Approximate learning with advice (CountingHeuristic)
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

        file << std::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n",
                            symbols,
                            dfa.states,
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
    std::cout << "problem automatas " << number_of_inproper_automata << "\n";
    std::cout << "ConnectivityTest Ends with Success\n";
}


} // namespace test
