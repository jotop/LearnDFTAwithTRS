#pragma once
#include "Lstar.h"

namespace test {

// Has any 'a' symbol
void test1()
{
    alphabet::Alphabet a;
    a.add_symbol({ .m_arity = 2, .m_name = "f" });
    a.add_symbol({ .m_arity = 1, .m_name = "g" });
    a.add_symbol({ .m_arity = 1, .m_name = "h" });
    a.add_symbol({ .m_arity = 0, .m_name = "a" });
    a.add_symbol({ .m_arity = 0, .m_name = "b" });
    a.add_symbol({ .m_arity = 0, .m_name = "c" });

    using bottom_up_dtfta::BottomUpDTFA;
    using bottom_up_dtfta::Transition;

    const BottomUpDTFA dfa =
          BottomUpDTFA(a,
                        2,
                        {
                              0
    })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "a", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "b", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "c", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0 }, .m_symbol = "g", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 1 }, .m_symbol = "g", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0 }, .m_symbol = "h", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 1 }, .m_symbol = "h", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 1, 1 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0, 1 }, .m_symbol = "f", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 1, 0 }, .m_symbol = "f", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 0, 0 }, .m_symbol = "f", .m_outgoing_state = 0 });

    lstar::Teacher t(dfa);
    lstar::Student s(t, a);
    s.learn();

    std::cout << s.teacher.get_notes();
}


// All subtrees have 'a' symbol
void test2()
{
    alphabet::Alphabet a;
    a.add_symbol({ .m_arity = 2, .m_name = "f" });
    a.add_symbol({ .m_arity = 1, .m_name = "g" });
    a.add_symbol({ .m_arity = 1, .m_name = "h" });
    a.add_symbol({ .m_arity = 0, .m_name = "a" });
    a.add_symbol({ .m_arity = 0, .m_name = "b" });
    a.add_symbol({ .m_arity = 0, .m_name = "c" });

    using bottom_up_dtfta::BottomUpDTFA;
    using bottom_up_dtfta::Transition;

    const BottomUpDTFA dfa =
          BottomUpDTFA(a,
                        2,
                        {
                              0
    })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "a", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "b", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "c", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0 }, .m_symbol = "g", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 1 }, .m_symbol = "g", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0 }, .m_symbol = "h", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 1 }, .m_symbol = "h", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 1, 1 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0, 1 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 1, 0 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0, 0 }, .m_symbol = "f", .m_outgoing_state = 0 });

    lstar::Teacher t(dfa);
    lstar::Student s(t, a);
    s.learn();

    std::cout << s.teacher.get_notes();
}


// All subtrees have 'a' symbol but symbol g reverts
void test3()
{
    alphabet::Alphabet a;
    a.add_symbol({ .m_arity = 2, .m_name = "f" });
    a.add_symbol({ .m_arity = 1, .m_name = "g" });
    a.add_symbol({ .m_arity = 1, .m_name = "h" });
    a.add_symbol({ .m_arity = 0, .m_name = "a" });
    a.add_symbol({ .m_arity = 0, .m_name = "b" });
    a.add_symbol({ .m_arity = 0, .m_name = "c" });

    using bottom_up_dtfta::BottomUpDTFA;
    using bottom_up_dtfta::Transition;

    const BottomUpDTFA dfa =
          BottomUpDTFA(a,
                        2,
                        {
                              0
    })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "a", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "b", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = {}, .m_symbol = "c", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0 }, .m_symbol = "g", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 1 }, .m_symbol = "g", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 0 }, .m_symbol = "h", .m_outgoing_state = 0 })
                .add_transition(Transition { .m_incoming_states = { 1 }, .m_symbol = "h", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 1, 1 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0, 1 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 1, 0 }, .m_symbol = "f", .m_outgoing_state = 1 })
                .add_transition(Transition { .m_incoming_states = { 0, 0 }, .m_symbol = "f", .m_outgoing_state = 0 });

    lstar::Teacher t(dfa);
    lstar::Student s(t, a);
    s.learn();

    std::cout << s.teacher.get_notes();
}


// Left path length is divisible by k
// f(x,y) -> f(x,a)
// f(x,y) -> g(x)
void test4(int k)
{
    alphabet::Alphabet a;
    a.add_symbol({ .m_arity = 2, .m_name = "f" });
    a.add_symbol({ .m_arity = 1, .m_name = "g" });
    a.add_symbol({ .m_arity = 1, .m_name = "h" });
    a.add_symbol({ .m_arity = 0, .m_name = "a" });
    a.add_symbol({ .m_arity = 0, .m_name = "b" });
    a.add_symbol({ .m_arity = 0, .m_name = "c" });

    using bottom_up_dtfta::BottomUpDTFA;
    using bottom_up_dtfta::Transition;

    BottomUpDTFA dfa = BottomUpDTFA(a, k, { 0 })
                              .add_transition(
                                    Transition { .m_incoming_states = {}, .m_symbol = "a", .m_outgoing_state = 1 })
                              .add_transition(
                                    Transition { .m_incoming_states = {}, .m_symbol = "b", .m_outgoing_state = 1 })
                              .add_transition(
                                    Transition { .m_incoming_states = {}, .m_symbol = "c", .m_outgoing_state = 1 });

    for (int i = 0; i < k; i++)
    {
        dfa.add_transition(Transition { .m_incoming_states = { i }, .m_symbol = "g", .m_outgoing_state = (i + 1) % k });
        dfa.add_transition(Transition { .m_incoming_states = { i }, .m_symbol = "h", .m_outgoing_state = (i + 1) % k });

        for (int j = 0; j < k + 1; j++)
        {
            dfa.add_transition(Transition {
                  .m_incoming_states = { i, j },
                  .m_symbol          = "f",
                  .m_outgoing_state  = (i + 1) % k
            });
        }
    }

    lstar::Teacher t(dfa);
    lstar::Teacher tadv(dfa);
    lstar::Student s(t, a);
    s.learn();

    std::cout << "No Advice\n" << s.teacher.get_notes();

    alphabet::Alphabet expanded_alphabet = a;
    expanded_alphabet.add_symbol({ 0, "T1" });
    expanded_alphabet.add_symbol({ 0, "T2" });

    TermRewritingSystem advice;
    tree::DAGTree       a1(expanded_alphabet);
    a1.add_node({}, "f");
    a1.add_node({ 0 }, "T1");
    a1.add_node({ 0 }, "T2");

    tree::DAGTree b1(expanded_alphabet);
    b1.add_node({}, "g");
    b1.add_node({ 0 }, { 0, "T1" });

    advice.add_rule(TermRule(2, a1, b1));

    lstar::StudentAdvice sadvice(tadv, a, advice);
    sadvice.learn();

    std::cout << "Advice\n" << sadvice.teacher.get_notes();
}

// Left path length is k
// f(x,y) -> f(x,a)
// f(x,y) -> g(x)
void test5(int k)
{
    k = k + 1;

    alphabet::Alphabet a;
    a.add_symbol({ .m_arity = 2, .m_name = "f" });
    a.add_symbol({ .m_arity = 1, .m_name = "g" });
    a.add_symbol({ .m_arity = 1, .m_name = "h" });
    a.add_symbol({ .m_arity = 0, .m_name = "a" });
    a.add_symbol({ .m_arity = 0, .m_name = "b" });
    a.add_symbol({ .m_arity = 0, .m_name = "c" });

    using bottom_up_dtfta::BottomUpDTFA;
    using bottom_up_dtfta::Transition;

    BottomUpDTFA dfa = BottomUpDTFA(a, k+1, { k-2 })
                              .add_transition(
                                    Transition { .m_incoming_states = {}, .m_symbol = "a", .m_outgoing_state = 0 })
                              .add_transition(
                                    Transition { .m_incoming_states = {}, .m_symbol = "b", .m_outgoing_state = 0 })
                              .add_transition(
                                    Transition { .m_incoming_states = {}, .m_symbol = "c", .m_outgoing_state = 0 });

    for (int i = 0; i < k; i++)
    {
        dfa.add_transition(Transition { .m_incoming_states = { i }, .m_symbol = "g", .m_outgoing_state = std::min(i + 1,k) });
        dfa.add_transition(
              Transition { .m_incoming_states = { i }, .m_symbol = "h", .m_outgoing_state = std::min(i + 1, k) });

        for (int j = 0; j < k + 1; j++)
        {
            dfa.add_transition(Transition {
                  .m_incoming_states = { i, j },
                  .m_symbol          = "f",
                  .m_outgoing_state  = std::min(i + 1, k)
            });
        }
    }

    lstar::Teacher t(dfa);
    lstar::Teacher tadv(dfa);
    lstar::Student s(t, a);
    s.learn();

    std::cout << "No Advice\n" << s.teacher.get_notes();

    alphabet::Alphabet expanded_alphabet = a;
    expanded_alphabet.add_symbol({ 0, "T1" });
    expanded_alphabet.add_symbol({ 0, "T2" });

    TermRewritingSystem advice;
    tree::DAGTree       a1(expanded_alphabet);
    a1.add_node({}, "f");
    a1.add_node({ 0 }, "T1");
    a1.add_node({ 0 }, "T2");

    tree::DAGTree b1(expanded_alphabet);
    b1.add_node({}, "g");
    b1.add_node({ 0 }, { 0, "T1" });

    advice.add_rule(TermRule(2, a1, b1));

    lstar::StudentAdvice sadvice(tadv, a, advice);
    sadvice.learn();

    std::cout << "Advice\n" << sadvice.teacher.get_notes();
}

// Second left path length is divisible by k - no sense to do it

// All path (from root to leaf) has length divisible by k
// ??

// Any path (from root to leaf) has length divisible by k --HARD-- exponentially many states
// ??


// rozdzielnosc

} // namespace test
