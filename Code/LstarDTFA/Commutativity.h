#pragma once
#include <chrono>
#include <fstream>
#include <random>

#include "BottomUpDTFA.h"
#include "Lstar.h"


namespace test {

bottom_up_dtfta::BottomUpDTFA random_commutativity_dfa(int states, int symbols)
{
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 2, .m_name = "foo" });
    for (int i = 0; i < symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);

    srand(std::time(nullptr));

    std::unordered_set<int> achieveable_s;
    std::vector<int>        achieveable_v;

    for (int i = 0; i < symbols; i++)
    {
        int outgoing = rand() % states;

        result.add_transition(bottom_up_dtfta::Transition { .m_incoming_states = {},
                                                            .m_symbol = std::string(1, static_cast<char>('a' + i)),
                                                            .m_outgoing_state = outgoing });
        achieveable_s.insert(outgoing);
        achieveable_v.emplace_back(outgoing);
    }

    std::vector<int> indices(states);
    std::iota(indices.begin(), indices.end(), 0); // Fill with 0 to n-1 (C++20 ranges)

    // Setup a random number generator
    std::random_device rd;
    std::mt19937       gen(rd());

    // Shuffle the vector randomly
    while (indices[0] == achieveable_v[0])
    {
        std::shuffle(indices.begin(), indices.end(), gen);
    }

    for (auto& i : indices)
    {
        bool available_trans = false;
        while (!available_trans)
        {
            int f = achieveable_v[rand() % achieveable_v.size()];
            int s = achieveable_v[rand() % achieveable_v.size()];

            available_trans = true;
            for (auto& t : result.get_delta_function())
            {
                if (t.m_symbol == "foo" && t.m_incoming_states == std::vector { f, s })
                {
                    available_trans = false;
                    break;
                }
            }

            if (available_trans)
            {
                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { f, s },
                      .m_symbol          = "foo",
                      .m_outgoing_state  = i
                });

                if (f != s)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, f },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
            }
        }

        achieveable_s.insert(i);
        achieveable_v.emplace_back(i);
    }

    for (int i = 0; i < states; i++)
    {
        for (int j = i; j < states; j++)
        {
            bool trans_exists = false;
            for (auto& t : result.get_delta_function())
            {
                if (t.m_symbol == "foo" && t.m_incoming_states == std::vector { i, j })
                {
                    trans_exists = true;
                    break;
                }
            }

            if (trans_exists)
            {
                continue;
            }

            int outgoing = rand() % states;

            result.add_transition(bottom_up_dtfta::Transition {
                  .m_incoming_states = { i, j },
                  .m_symbol          = "foo",
                  .m_outgoing_state  = outgoing
            });

            if (i != j)
            {
                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { j, i },
                      .m_symbol          = "foo",
                      .m_outgoing_state  = outgoing
                });
            }
        }
    }

    std::unordered_set<int> win;
    for (int i = 0; i < (states + 9) / 10; i++)
    {
        auto cand = rand() % states;
        if (!win.contains(cand))
        {
            result.set_final_state(cand);
        }
    }

    result.set_states(states);

    return result;
}


void CommutativityTest()
{
    std::cout << "\n\nCommutativityTest\n";

    std::ofstream file("CommutativityTestResults.csv");
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create file CommutativityTestResults.csv\n";
        return;
    }

    file << "Number of leaf symbols,States in tree automata,Standard time,Standard membership queries,Standard equivalence queries,Standard automata found states number,Advice time,Advice membership queries,Advice equivalence queries,Advice automata found states number\n";

    int number_of_problems_automatas = 0;

    for (int symbols = 2; symbols < 3; symbols++)
    {
        std::cout << std::format("Number of symbols: {}\n", symbols);

        for (int states = 50; states < 51; states += 10)
        {
            for (int probe = 0; probe < 10; probe++)
            {
                auto tree_automata = random_commutativity_dfa(states, symbols);

                std::cout << std::format("\nTree Automata states {}\n", tree_automata.get_states());

                //
                lstar::Teacher t(tree_automata);
                lstar::Teacher tadv(tree_automata);

                lstar::Student s(t, tree_automata.get_alphabet());
                auto           start = std::chrono::high_resolution_clock::now();
                if (!s.learn().has_value())
                {
                    number_of_problems_automatas++;
                    continue;
                }
                auto stop    = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

                std::cout << "No Advice " << elapsed << "\n" << s.teacher.get_notes();

                alphabet::Alphabet expanded_alphabet = tree_automata.get_alphabet();
                expanded_alphabet.add_symbol({ 0, "T1" });
                expanded_alphabet.add_symbol({ 0, "T2" });

                TermRewritingSystem advice;
                tree::DAGTree       a1(expanded_alphabet);
                a1.add_node({}, "foo");
                a1.add_node({ 0 }, "T1");
                a1.add_node({ 0 }, "T2");

                tree::DAGTree b1(expanded_alphabet);
                b1.add_node({}, "foo");
                b1.add_node({ 0 }, "T2");
                b1.add_node({ 0 }, "T1");

                advice.add_rule(TermRule(2, a1, b1));

                lstar::StudentAdvice sadvice(tadv, tree_automata.get_alphabet(), advice);
                start = std::chrono::high_resolution_clock::now();
                if (!sadvice.learn().has_value())
                {
                    number_of_problems_automatas++;
                    continue;
                }
                stop             = std::chrono::high_resolution_clock::now();
                auto elapsed_adv = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

                std::cout << "Advice " << elapsed_adv << "\n" << sadvice.teacher.get_notes();

                file << std::format("{},{},{},{},{},{},{},{},{},{}\n",
                                    symbols,
                                    tree_automata.get_states(),
                                    elapsed.count(),
                                    s.teacher.get_number_mem(),
                                    s.teacher.get_number_eq(),
                                    s.teacher.get_result_states(),
                                    elapsed_adv.count(),
                                    sadvice.teacher.get_number_mem(),
                                    sadvice.teacher.get_number_eq(),
                                    sadvice.teacher.get_result_states());
            }
        }
    }

    file.close();
    std::cout << "problem automatas " << number_of_problems_automatas << "\n";
    std::cout << "CommutativityTest Ends with Success\n";
}


bottom_up_dtfta::BottomUpDTFA random_commutativity_dfa3ar(int states, int symbols)
{
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 3, .m_name = "foo" });
    for (int i = 0; i < symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);

    srand(std::time(nullptr));

    std::unordered_set<int> achieveable_s;
    std::vector<int>        achieveable_v;

    for (int i = 0; i < symbols; i++)
    {
        int outgoing = rand() % states;

        result.add_transition(bottom_up_dtfta::Transition { .m_incoming_states = {},
                                                            .m_symbol = std::string(1, static_cast<char>('a' + i)),
                                                            .m_outgoing_state = outgoing });
        if (!achieveable_s.contains(outgoing))
        {
            achieveable_s.insert(outgoing);
            achieveable_v.emplace_back(outgoing);
        }
    }

    std::vector<int> indices(states);
    std::iota(indices.begin(), indices.end(), 0); // Fill with 0 to n-1 (C++20 ranges)

    // Setup a random number generator
    std::random_device rd;
    std::mt19937       gen(rd());

    // Shuffle the vector randomly
    while (indices[0] == achieveable_v[0])std::shuffle(indices.begin(), indices.end(), gen);

    for (auto& i : indices)
    {
        bool available_trans = false;
        while (!available_trans)
        {
            int f = achieveable_v[rand() % achieveable_v.size()];
            int s = achieveable_v[rand() % achieveable_v.size()];
            int t = achieveable_v[rand() % achieveable_v.size()];

            available_trans = true;
            for (auto& tr : result.get_delta_function())
            {
                if (tr.m_symbol == "foo" && tr.m_incoming_states == std::vector { f, s, t })
                {
                    available_trans = false;
                    break;
                }
            }

            if (available_trans)
            {
                if (f == s && f == t && s == t)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, s, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
                else if (f != s && f != t && s != t)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, s, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, t, s },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, f, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, t, f },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { t, s, f },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { t, f, s },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
                else if (f == s)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, s, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, t, s },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });


                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { t, s, f },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
                else if (f == t)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, s, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, t, s },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, f, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
                else if (s == t)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { f, s, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, f, t },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, t, f },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
            }
        }

        achieveable_s.insert(i);
        achieveable_v.emplace_back(i);
    }

    for (int i = 0; i < states; i++)
    {
        for (int j = i; j < states; j++)
        {
            for (int k = j; k < states; k++)
            {
                bool trans_exists = false;
                for (auto& t : result.get_delta_function())
                {
                    if (t.m_symbol == "foo" && t.m_incoming_states == std::vector { i, j, k })
                    {
                        trans_exists = true;
                        break;
                    }
                }

                if (trans_exists)
                {
                    continue;
                }

                int outgoing = rand() % states;

                if (i == j && i == k && j == k)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, j, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });
                }
                else if (i != j && i != k && j != k)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, j, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, k, j },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { j, i, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { j, k, i },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { k, j, i },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { k, i, j },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });
                }
                else if (i == j)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, j, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, k, j },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });


                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { k, j, i },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });
                }
                else if (i == k)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, j, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, k, j },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { j, i, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });
                }
                else if (j == k)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { i, j, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { j, i, k },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });

                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { j, k, i },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = outgoing
                    });
                }
            }
        }
    }

    std::unordered_set<int> win;
    for (int i = 0; i < (states + 9) / 10; i++)
    {
        auto cand = rand() % states;
        if (!win.contains(cand))
        {
            result.set_final_state(cand);
            win.insert(cand);
        }
    }

    result.set_states(states);

    return result;
}


void CommutativityTest3ar()
{
    std::cout << "\n\nCommutativityTest\n";

    std::ofstream file("CommutativityTestResults3ar.csv");
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create file CommutativityTestResults.csv\n";
        return;
    }

    file << "Number of leaf symbols,States in tree automata,Standard time,Standard membership queries,Standard equivalence queries,Standard automata found states number,Advice time,Advice membership queries,Advice equivalence queries,Advice automata found states number\n";

    int number_of_problems_automatas = 0;

    for (int symbols = 2; symbols < 10; symbols++)
    {
        std::cout << std::format("Number of symbols: {}\n", symbols);

        for (int states = 10; states < 20; states += 1)
        {
            for (int probe = 0; probe < 2; probe++)
            {
                auto tree_automata = random_commutativity_dfa3ar(states, symbols);

                std::cout << std::format("\nTree Automata states {}\n", tree_automata.get_states());

                //
                lstar::Teacher t(tree_automata);
                lstar::Teacher tadv(tree_automata);

                lstar::Student s(t, tree_automata.get_alphabet());
                auto           start = std::chrono::high_resolution_clock::now();
                if (!s.learn().has_value())
                {
                    number_of_problems_automatas++;
                    continue;
                }
                auto stop    = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

                std::cout << "No Advice " << elapsed << "\n" << s.teacher.get_notes();

                alphabet::Alphabet expanded_alphabet = tree_automata.get_alphabet();
                expanded_alphabet.add_symbol({ 0, "T1" });
                expanded_alphabet.add_symbol({ 0, "T2" });
                expanded_alphabet.add_symbol({ 0, "T3" });

                TermRewritingSystem advice;
                tree::DAGTree       a1(expanded_alphabet);
                a1.add_node({}, "foo");
                a1.add_node({ 0 }, "T1");
                a1.add_node({ 0 }, "T2");
                a1.add_node({ 0 }, "T3");

                tree::DAGTree b1(expanded_alphabet);
                b1.add_node({}, "foo");
                b1.add_node({ 0 }, "T1");
                b1.add_node({ 0 }, "T3");
                b1.add_node({ 0 }, "T2");
                advice.add_rule(TermRule(3, a1, b1));

                tree::DAGTree b2(expanded_alphabet);
                b2.add_node({}, "foo");
                b2.add_node({ 0 }, "T3");
                b2.add_node({ 0 }, "T1");
                b2.add_node({ 0 }, "T2");
                advice.add_rule(TermRule(3, a1, b2));

                tree::DAGTree b3(expanded_alphabet);
                b3.add_node({}, "foo");
                b3.add_node({ 0 }, "T3");
                b3.add_node({ 0 }, "T2");
                b3.add_node({ 0 }, "T1");
                advice.add_rule(TermRule(3, a1, b3));

                tree::DAGTree b4(expanded_alphabet);
                b4.add_node({}, "foo");
                b4.add_node({ 0 }, "T2");
                b4.add_node({ 0 }, "T1");
                b4.add_node({ 0 }, "T3");
                advice.add_rule(TermRule(3, a1, b4));

                tree::DAGTree b5(expanded_alphabet);
                b5.add_node({}, "foo");
                b5.add_node({ 0 }, "T2");
                b5.add_node({ 0 }, "T3");
                b5.add_node({ 0 }, "T1");
                advice.add_rule(TermRule(3, a1, b5));

                lstar::StudentAdvice sadvice(tadv, tree_automata.get_alphabet(), advice);
                start = std::chrono::high_resolution_clock::now();
                if (!sadvice.learn().has_value())
                {
                    number_of_problems_automatas++;
                    continue;
                }
                stop             = std::chrono::high_resolution_clock::now();
                auto elapsed_adv = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

                std::cout << "Advice " << elapsed << "\n" << sadvice.teacher.get_notes();

                file << std::format("{},{},{},{},{},{},{},{},{},{}\n",
                                    symbols,
                                    tree_automata.get_states(),
                                    elapsed.count(),
                                    s.teacher.get_number_mem(),
                                    s.teacher.get_number_eq(),
                                    s.teacher.get_result_states(),
                                    elapsed_adv.count(),
                                    sadvice.teacher.get_number_mem(),
                                    sadvice.teacher.get_number_eq(),
                                    sadvice.teacher.get_result_states());
            }
        }
    }

    file.close();
    std::cout << "problem automatas " << number_of_problems_automatas << "\n";
    std::cout << "CommutativityTest Ends with Success\n";
}



bottom_up_dtfta::BottomUpDTFA random_commutativity_dfa_as(int states, int symbols)
{
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 2, .m_name = "foo" });
    alph.add_symbol({ .m_arity = 2, .m_name = "g" });
    for (int i = 0; i < symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);

    srand(std::time(nullptr));

    std::unordered_set<int> achieveable_s;
    std::vector<int>        achieveable_v;

    for (int i = 0; i < symbols; i++)
    {
        int outgoing = rand() % states;

        result.add_transition(bottom_up_dtfta::Transition { .m_incoming_states = {},
                                                            .m_symbol = std::string(1, static_cast<char>('a' + i)),
                                                            .m_outgoing_state = outgoing });
        achieveable_s.insert(outgoing);
        achieveable_v.emplace_back(outgoing);
    }

    std::vector<int> indices(states);
    std::iota(indices.begin(), indices.end(), 0); // Fill with 0 to n-1 (C++20 ranges)

    // Setup a random number generator
    std::random_device rd;
    std::mt19937       gen(rd());

    // Shuffle the vector randomly
    while (indices[0] == achieveable_v[0])
    {
        std::shuffle(indices.begin(), indices.end(), gen);
    }

    for (auto& i : indices)
    {
        bool available_trans = false;
        while (!available_trans)
        {
            int f = achieveable_v[rand() % achieveable_v.size()];
            int s = achieveable_v[rand() % achieveable_v.size()];

            available_trans = true;
            for (auto& t : result.get_delta_function())
            {
                if (t.m_symbol == "foo" && t.m_incoming_states == std::vector { f, s })
                {
                    available_trans = false;
                    break;
                }
            }

            if (available_trans)
            {
                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { f, s },
                      .m_symbol          = "foo",
                      .m_outgoing_state  = i
                });

                if (f != s)
                {
                    result.add_transition(bottom_up_dtfta::Transition {
                          .m_incoming_states = { s, f },
                          .m_symbol          = "foo",
                          .m_outgoing_state  = i
                    });
                }
            }
        }

        achieveable_s.insert(i);
        achieveable_v.emplace_back(i);
    }

    for (int i = 0; i < states; i++)
    {
        for (int j = i; j < states; j++)
        {
            bool trans_exists = false;
            for (auto& t : result.get_delta_function())
            {
                if (t.m_symbol == "foo" && t.m_incoming_states == std::vector { i, j })
                {
                    trans_exists = true;
                    break;
                }
            }

            if (trans_exists)
            {
                continue;
            }

            int outgoing = rand() % states;

            result.add_transition(bottom_up_dtfta::Transition {
                  .m_incoming_states = { i, j },
                  .m_symbol          = "foo",
                  .m_outgoing_state  = outgoing
            });

            if (i != j)
            {
                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { j, i },
                      .m_symbol          = "foo",
                      .m_outgoing_state  = outgoing
                });
            }
        }
    }

    for (int i = 0; i < states; i++)
    {
        for (int j = i; j < states; j++)
        {
            int outgoing = rand() % states;

            result.add_transition(bottom_up_dtfta::Transition {
                  .m_incoming_states = { i, j },
                  .m_symbol          = "g",
                  .m_outgoing_state  = outgoing
            });

            if (i != j)
            {
                result.add_transition(bottom_up_dtfta::Transition {
                      .m_incoming_states = { j, i },
                      .m_symbol          = "g",
                      .m_outgoing_state  = outgoing
                });
            }
        }
    }

    std::unordered_set<int> win;
    for (int i = 0; i < (states + 9) / 10; i++)
    {
        auto cand = rand() % states;
        if (!win.contains(cand))
        {
            result.set_final_state(cand);
        }
    }

    result.set_states(states);

    return result;
}


void CommutativityTestAdditionalSymbol()
{
    std::cout << "\n\nCommutativityTest\n";

    std::ofstream file("CommutativityTestResultsAdditionalSymbol.csv");
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create file CommutativityTestResults.csv\n";
        return;
    }

    file << "Number of leaf symbols,States in tree automata,Standard time,Standard membership queries,Standard equivalence queries,Standard automata found states number,Advice time,Advice membership queries,Advice equivalence queries,Advice automata found states number\n";

    int number_of_problems_automatas = 0;

    for (int symbols = 2; symbols < 3; symbols++)
    {
        std::cout << std::format("Number of symbols: {}\n", symbols);

        for (int states = 30; states < 51; states += 10)
        {
            for (int probe = 0; probe < 4; probe++)
            {
                auto tree_automata = random_commutativity_dfa_as(states, symbols);

                std::cout << std::format("\nTree Automata states {}\n", tree_automata.get_states());

                //
                lstar::Teacher t(tree_automata);
                lstar::Teacher tadv(tree_automata);

                lstar::Student s(t, tree_automata.get_alphabet());
                auto           start = std::chrono::high_resolution_clock::now();
                if (!s.learn().has_value())
                {
                    number_of_problems_automatas++;
                    continue;
                }
                auto stop    = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

                std::cout << "No Advice " << elapsed << "\n" << s.teacher.get_notes();

                alphabet::Alphabet expanded_alphabet = tree_automata.get_alphabet();
                expanded_alphabet.add_symbol({ 0, "T1" });
                expanded_alphabet.add_symbol({ 0, "T2" });

                TermRewritingSystem advice;
                tree::DAGTree       a1(expanded_alphabet);
                a1.add_node({}, "foo");
                a1.add_node({ 0 }, "T1");
                a1.add_node({ 0 }, "T2");

                tree::DAGTree b1(expanded_alphabet);
                b1.add_node({}, "foo");
                b1.add_node({ 0 }, "T2");
                b1.add_node({ 0 }, "T1");

                advice.add_rule(TermRule(2, a1, b1));

                lstar::StudentAdvice sadvice(tadv, tree_automata.get_alphabet(), advice);
                start = std::chrono::high_resolution_clock::now();
                if (!sadvice.learn().has_value())
                {
                    number_of_problems_automatas++;
                    continue;
                }
                stop             = std::chrono::high_resolution_clock::now();
                auto elapsed_adv = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

                std::cout << "Advice " << elapsed_adv << "\n" << sadvice.teacher.get_notes();

                file << std::format("{},{},{},{},{},{},{},{},{},{}\n",
                                    symbols,
                                    tree_automata.get_states(),
                                    elapsed.count(),
                                    s.teacher.get_number_mem(),
                                    s.teacher.get_number_eq(),
                                    s.teacher.get_result_states(),
                                    elapsed_adv.count(),
                                    sadvice.teacher.get_number_mem(),
                                    sadvice.teacher.get_number_eq(),
                                    sadvice.teacher.get_result_states());
            }
        }
    }

    file.close();
    std::cout << "problem automatas " << number_of_problems_automatas << "\n";
    std::cout << "CommutativityTest Ends with Success\n";
}


} // namespace test
