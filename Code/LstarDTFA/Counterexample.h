#pragma once
#include "BottomUpDTFA.h"


namespace counterexample {

using bottom_up_dtfta::BottomUpDTFA;
using bottom_up_dtfta::Transition;
using tree::DAGTree;


std::optional<DAGTree> find_counterexample(const BottomUpDTFA& dfa1, const BottomUpDTFA& dfa2)
{
    int dfa1_states = dfa1.get_states();
    int dfa2_states = dfa2.get_states();

    auto get_index = [&](const int i, const int j) -> int
    {
        return i * dfa2_states + j;
    };

    std::vector<std::optional<std::pair<Transition, Transition>>> qq(dfa1.get_states() * dfa2.get_states());

    std::function<DAGTree(const int)>   build_counterexample;
    std::vector<std::optional<DAGTree>> remembered_ctx(dfa1_states * dfa2_states, std::nullopt);

    build_counterexample = [&](const int index) -> DAGTree
    {
        if (remembered_ctx[index].has_value())
        {
            return remembered_ctx[index].value();
        }

        DAGTree res(dfa1.get_alphabet());

        auto pair_of_transition = qq[index].value();
        auto root               = res.add_node({}, pair_of_transition.first.m_symbol);

        for (size_t i = 0; i < pair_of_transition.first.m_incoming_states.size(); i++)
        {
            res.join_tree({ root },
                          build_counterexample(get_index(pair_of_transition.first.m_incoming_states[i],
                                                         pair_of_transition.second.m_incoming_states[i])));
        }

        remembered_ctx[index] = res;
        //res.dagify_bottom_up();
        return res;
    };

    //auto start_build_graph = std::chrono::high_resolution_clock::now();
    bool was_improved      = true;
    while (was_improved)
    {
        was_improved = false;
        for (auto& symbol : dfa1.get_alphabet().get_symbols())
        {
            for (const auto& tr1 : dfa1.get_delta_function(symbol.m_name))
            {
                for (const auto& tr2 : dfa2.get_delta_function(symbol.m_name))
                {
                    auto index = get_index(tr1.m_outgoing_state, tr2.m_outgoing_state);
                    if (qq[index].has_value())
                    {
                        continue;
                    }

                    bool childs_are_ready = true;
                    for (size_t i = 0; i < tr1.m_incoming_states.size(); i++)
                    {
                        if (!qq[get_index(tr1.m_incoming_states[i], tr2.m_incoming_states[i])].has_value())
                        {
                            childs_are_ready = false;
                            break;
                        }
                    }

                    if (childs_are_ready)
                    {
                        qq[index] = std::make_pair(tr1, tr2);

                        if (dfa1.is_accepted_state(tr1.m_outgoing_state)
                            != dfa2.is_accepted_state(tr2.m_outgoing_state))
                        {
                            auto ctx = build_counterexample(index);
                            ctx.dagify_bottom_up();
                            return ctx;
                        }

                        was_improved = true;
                    }
                }
            }
        }
    }

    //auto stop_build_graph    = std::chrono::high_resolution_clock::now();
    //auto elapsed_build_graph = std::chrono::duration_cast<std::chrono::seconds>(stop_build_graph - start_build_graph);
    //if (elapsed_build_graph.count() > 5)
    //{
    //    std::cout << "Build Ctx building graph " << elapsed_build_graph << "\n";
    //}


    //for (int state1 = 0; state1 < dfa1_states; state1++)
    //{
    //    for (int state2 = 0; state2 < dfa2_states; state2++)
    //    {
    //        if (dfa1.is_accepted_state(state1) != dfa2.is_accepted_state(state2))
    //        {
    //            auto counterexample_index = get_index(state1, state2);
    //            if (qq[counterexample_index].has_value())
    //            {
    //                auto ctx = build_counterexample(counterexample_index);
    //                ctx.dagify_bottom_up();
    //                return ctx;
    //            }
    //        }
    //    }
    //}

    return std::nullopt;
}
} // namespace counterexample
