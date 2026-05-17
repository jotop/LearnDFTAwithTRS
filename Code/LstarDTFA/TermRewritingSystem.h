#pragma once
#include <random>

#include "BitMatrix.h"
#include "BottomUpDTFA.h"
#include "ContextDAGTree.h"
#include "DAGTree.h"


class TermRule
{
    int           number_of_terms;
    // Terms: T1, T2, T3, ..., TN
    tree::DAGTree left_tree;
    tree::DAGTree right_tree;

    int get_state_left(std::vector<int>& tuple, bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        return get_state(left_tree, tuple, dfa);
    }

    int get_state_right(std::vector<int>& tuple, bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        return get_state(right_tree, tuple, dfa);
    }

    static int get_state(tree::DAGTree& tree, std::vector<int>& tuple, bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        return get_state(tree, tuple, dfa, 0);
    }

    static int get_state(tree::DAGTree&                  tree,
                         std::vector<int>&               tuple,
                         bottom_up_dtfta::BottomUpDTFA& dfa,
                         tree::NodeId                    node)
    {
        if (tree.nodes[node].childs.empty())
        {
            if (tree.nodes[node].symbol.m_name.starts_with("T"))
            {
                auto index = atoi(tree.nodes[node].symbol.m_name.substr(1).c_str());
                return tuple[index - 1];
            }
            return dfa.get_state_zero_arity(tree.nodes[node].symbol.m_name);
        }

        std::vector<int> child_states;
        for (auto c : tree.nodes[node].childs)
        {
            child_states.emplace_back(get_state(tree, tuple, dfa, c));
        }

        return dfa.get_state(child_states, tree.nodes[node].symbol).value_or(-1);
    }

    static tree::DAGTree get_tree(tree::DAGTree& tree, std::vector<tree::DAGTree>& tuple)
    {
        tree::DAGTree& result_tree = tree;

        bool was_inserted = true;
        while (was_inserted)
        {
            was_inserted = false;
            for (auto& node : result_tree.nodes)
            {
                if (node.childs.empty() && node.symbol.m_name.starts_with("T"))
                {
                    auto index   = atoi(node.symbol.m_name.substr(1).c_str());
                    result_tree  = result_tree.hitch_subtree_as_node(tuple[index - 1], node.id);
                    was_inserted = true;
                    break;
                }
            }
        }

        //result_tree.dagify_bottom_up();
        return result_tree;
    }


    static std::optional<tree::DAGTree> return_counterexample(
          const tree::DAGTree&                                                                    t,
          const std::vector<int>&                                                                 tuple,
          const std::vector<std::pair<bottom_up_dtfta::Transition, bottom_up_dtfta::Transition>>& path,
          bottom_up_dtfta::BottomUpDTFA&                                                         dfa)
    {
        tree::DAGTree previous = t;

        bool was_improved = true;
        while (was_improved)
        {
            was_improved = false;
            for (auto& n : previous.nodes)
            {
                if (n.symbol.m_name.starts_with("T"))
                {
                    auto index      = atoi(n.symbol.m_name.substr(1).c_str());
                    auto child_tree = dfa.get_small_tree(tuple[index - 1]);
                    if (!child_tree.has_value())
                    {
                        return std::nullopt;
                    }
                    previous     = previous.hitch_subtree_as_node(child_tree.value(), n.id);
                    was_improved = true;
                    break;
                }
            }
        }

        for (auto& step : path)
        {
            auto tr1 = step.first;
            auto tr2 = step.second;

            tree::DAGTree tree(t.get_alphabet());
            tree.add_node({}, tr1.m_symbol);

            for (size_t i = 0; i < tr1.m_incoming_states.size(); i++)
            {
                if (tr1.m_incoming_states[i] == tr2.m_incoming_states[i])
                {
                    auto child_tree = dfa.get_small_tree(tr1.m_incoming_states[i]);
                    if (!child_tree.has_value())
                    {
                        return std::nullopt;
                    }
                    tree.join_tree({ 0 }, child_tree.value());
                    continue;
                }

                tree.join_tree({ 0 }, previous);
            }
            previous = tree;
        }

        previous.dagify_bottom_up();
        return previous;
    }

    std::pair<std::optional<tree::DAGTree>, std::optional<tree::DAGTree>> return_counterexample(
          const std::vector<int>&                                                                 tuple,
          const std::vector<std::pair<bottom_up_dtfta::Transition, bottom_up_dtfta::Transition>>& path,
          bottom_up_dtfta::BottomUpDTFA&                                                         dfa) const
    {
        return std::make_pair(return_counterexample(left_tree, tuple, path, dfa),
                              return_counterexample(right_tree, tuple, path, dfa));
    }


public:
    TermRule(int n, tree::DAGTree& l, tree::DAGTree& r)
          : number_of_terms(n),
            left_tree(l),
            right_tree(r)
    {}

    struct CounterexampleCore
    {
        int              q1;
        int              q2;
        std::vector<int> tuple;
    };

    tree::DAGTree get_left_tree(std::vector<tree::DAGTree>& tuple)
    {
        return get_tree(left_tree, tuple);
    }

    tree::DAGTree get_right_tree(std::vector<tree::DAGTree>& tuple)
    {
        return get_tree(right_tree, tuple);
    }

    std::vector<CounterexampleCore> get_ctx_cores(bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        std::vector<int> states_tuple(number_of_terms + 1);

        std::vector<CounterexampleCore> ctx_cores;

        auto gen_all_tuples = [&](auto&& self, int pos) -> void
        {
            if (pos == number_of_terms)
            {
                auto ls = get_state_left(states_tuple, dfa);
                auto rs = get_state_right(states_tuple, dfa);

                if (rs != ls && rs >= 0 && ls >= 0)
                {
                    ctx_cores.emplace_back(CounterexampleCore { .q1 = ls, .q2 = rs, .tuple = states_tuple });
                }

                return;
            }

            // TODO was number_of_terms possibly bug
            for (int i = 0; i < dfa.get_states(); ++i)
            {
                states_tuple[pos] = i;
                self(self, pos + 1);
            }
        };
        gen_all_tuples(gen_all_tuples, 0);

        return ctx_cores;
    }

    /// Random version: checks only num_samples random tuples instead of all combinations.
    std::vector<CounterexampleCore> get_ctx_cores_random(bottom_up_dtfta::BottomUpDTFA& dfa,
                                                         int num_samples, std::mt19937& gen)
    {
        std::vector<CounterexampleCore> ctx_cores;
        std::uniform_int_distribution<int> dist(0, dfa.get_states() - 1);

        for (int sample = 0; sample < num_samples; ++sample)
        {
            std::vector<int> states_tuple(number_of_terms + 1);
            for (int i = 0; i < number_of_terms; ++i)
            {
                states_tuple[i] = dist(gen);
            }

            auto ls = get_state_left(states_tuple, dfa);
            auto rs = get_state_right(states_tuple, dfa);

            if (rs != ls && rs >= 0 && ls >= 0)
            {
                ctx_cores.emplace_back(CounterexampleCore { .q1 = ls, .q2 = rs, .tuple = states_tuple });
            }
        }

        return ctx_cores;
    }

    /// Counting heuristic: counts how many tuples lead to each state for left vs right.
    /// Returns true if distributions differ (meaning there IS an inconsistency).
    /// Computes state distributions recursively over the tree structure,
    /// combining child distributions through DFA transitions at each internal node.
    bool has_counting_difference(bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        int num_states = dfa.get_states();
        if (num_states == 0) return false;

        auto left_counts  = count_states_for_tree(left_tree, dfa);
        auto right_counts = count_states_for_tree(right_tree, dfa);

        return left_counts != right_counts;
    }

private:
    /// Computes for a tree the distribution: for each state q, how many
    /// variable assignments cause the tree to evaluate to q.
    /// Handles variable sharing by enumerating only non-linear variables externally.
    std::vector<long long> count_states_for_tree(tree::DAGTree& tree, bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        int num_states = dfa.get_states();

        // Find which variables appear in this tree and how many times
        std::vector<int> var_occurrences(number_of_terms, 0);
        count_variable_occurrences(tree, 0, var_occurrences);

        // Identify non-linear variables (appear more than once)
        std::vector<int> non_linear_vars;
        for (int i = 0; i < number_of_terms; ++i)
        {
            if (var_occurrences[i] > 1)
            {
                non_linear_vars.push_back(i);
            }
        }

        // Count absent variables (not in this tree but in the rule)
        int num_absent_vars = 0;
        for (int i = 0; i < number_of_terms; ++i)
        {
            if (var_occurrences[i] == 0)
            {
                num_absent_vars++;
            }
        }

        // Multiplier for absent variables: each contributes |Q| factor
        long long absent_multiplier = 1;
        for (int i = 0; i < num_absent_vars; ++i)
        {
            absent_multiplier *= num_states;
        }

        if (non_linear_vars.empty())
        {
            // All variables are linear or absent: compute fully recursively
            auto counts = count_node_recursive(tree, dfa, 0, var_occurrences);
            for (auto& c : counts)
            {
                c *= absent_multiplier;
            }
            return counts;
        }

        // Non-linear variables exist: enumerate their assignments,
        // compute recursively for each fixed assignment
        std::vector<long long> total_counts(num_states, 0);
        std::vector<int> fixed_assignment(number_of_terms, -1);

        auto enumerate_nonlinear = [&](auto&& self, int idx) -> void
        {
            if (idx == static_cast<int>(non_linear_vars.size()))
            {
                auto counts = count_node_recursive_fixed(tree, dfa, 0, var_occurrences, fixed_assignment);
                for (int q = 0; q < num_states; ++q)
                {
                    total_counts[q] += counts[q] * absent_multiplier;
                }
                return;
            }

            int var = non_linear_vars[idx];
            for (int s = 0; s < num_states; ++s)
            {
                fixed_assignment[var] = s;
                self(self, idx + 1);
            }
        };
        enumerate_nonlinear(enumerate_nonlinear, 0);

        return total_counts;
    }

    void count_variable_occurrences(tree::DAGTree& tree, tree::NodeId node, std::vector<int>& occurrences)
    {
        if (tree.nodes[node].childs.empty())
        {
            if (tree.nodes[node].symbol.m_name.starts_with("T"))
            {
                auto index = atoi(tree.nodes[node].symbol.m_name.substr(1).c_str());
                occurrences[index - 1]++;
            }
            return;
        }
        for (auto c : tree.nodes[node].childs)
        {
            count_variable_occurrences(tree, c, occurrences);
        }
    }

    /// Counts linear variables (occurrence == 1) in a subtree.
    int count_linear_vars_in_subtree(tree::DAGTree& tree, tree::NodeId node, const std::vector<int>& var_occurrences)
    {
        if (tree.nodes[node].childs.empty())
        {
            if (tree.nodes[node].symbol.m_name.starts_with("T"))
            {
                auto index = atoi(tree.nodes[node].symbol.m_name.substr(1).c_str());
                if (var_occurrences[index - 1] == 1)
                {
                    return 1;
                }
            }
            return 0;
        }
        int count = 0;
        for (auto c : tree.nodes[node].childs)
        {
            count += count_linear_vars_in_subtree(tree, c, var_occurrences);
        }
        return count;
    }

    /// Recursive count for trees where all variables are linear (each appears at most once).
    /// Returns vector of size |Q| where entry[q] = number of linear-variable assignments
    /// that make this subtree evaluate to state q.
    std::vector<long long> count_node_recursive(tree::DAGTree& tree, bottom_up_dtfta::BottomUpDTFA& dfa,
                                                 tree::NodeId node, const std::vector<int>& var_occurrences)
    {
        int num_states = dfa.get_states();

        if (tree.nodes[node].childs.empty())
        {
            if (tree.nodes[node].symbol.m_name.starts_with("T"))
            {
                // Linear variable: uniform distribution
                // Other linear vars in the full tree (not in this subtree) are free
                int total_linear = count_linear_vars_in_subtree(tree, 0, var_occurrences);
                long long factor = 1;
                for (int i = 0; i < total_linear - 1; ++i)
                {
                    factor *= num_states;
                }
                return std::vector<long long>(num_states, factor);
            }
            else
            {
                // Constant: always one state
                int state = dfa.get_state_zero_arity(tree.nodes[node].symbol.m_name);
                int total_linear = count_linear_vars_in_subtree(tree, 0, var_occurrences);
                long long factor = 1;
                for (int i = 0; i < total_linear; ++i)
                {
                    factor *= num_states;
                }
                std::vector<long long> counts(num_states, 0);
                if (state >= 0 && state < num_states)
                {
                    counts[state] = factor;
                }
                return counts;
            }
        }

        // Internal node: get child distributions and combine through DFA transitions
        auto& children = tree.nodes[node].childs;
        int arity = static_cast<int>(children.size());

        std::vector<std::vector<long long>> child_dists(arity);
        for (int i = 0; i < arity; ++i)
        {
            child_dists[i] = count_node_recursive(tree, dfa, children[i], var_occurrences);
        }

        // Combine: enumerate child state combinations
        std::vector<long long> result(num_states, 0);
        std::vector<int> child_state_tuple(arity, 0);

        auto combine = [&](auto&& self, int pos) -> void
        {
            if (pos == arity)
            {
                long long product = 1;
                for (int i = 0; i < arity; ++i)
                {
                    product *= child_dists[i][child_state_tuple[i]];
                    if (product == 0) return;
                }
                auto parent_state = dfa.get_state(child_state_tuple, tree.nodes[node].symbol);
                if (parent_state.has_value() && parent_state.value() >= 0 && parent_state.value() < num_states)
                {
                    result[parent_state.value()] += product;
                }
                return;
            }
            for (int s = 0; s < num_states; ++s)
            {
                child_state_tuple[pos] = s;
                self(self, pos + 1);
            }
        };
        combine(combine, 0);

        // Account for linear vars NOT in this subtree (they are free)
        int linear_in_subtree = count_linear_vars_in_subtree(tree, node, var_occurrences);
        int total_linear = count_linear_vars_in_subtree(tree, 0, var_occurrences);
        int linear_outside = total_linear - linear_in_subtree;
        long long outside_factor = 1;
        for (int i = 0; i < linear_outside; ++i)
        {
            outside_factor *= num_states;
        }
        // Actually the child distributions already account for all linear vars
        // since each leaf contributes factor for (total_linear - 1) other vars.
        // The combination at internal nodes multiplies these correctly.
        // No additional factor needed here.

        return result;
    }

    /// Recursive count with non-linear variables fixed to specific states.
    /// Linear variables still contribute uniformly.
    std::vector<long long> count_node_recursive_fixed(tree::DAGTree& tree, bottom_up_dtfta::BottomUpDTFA& dfa,
                                                       tree::NodeId node, const std::vector<int>& var_occurrences,
                                                       const std::vector<int>& fixed_assignment)
    {
        int num_states = dfa.get_states();

        if (tree.nodes[node].childs.empty())
        {
            if (tree.nodes[node].symbol.m_name.starts_with("T"))
            {
                auto index = atoi(tree.nodes[node].symbol.m_name.substr(1).c_str());
                int var_idx = index - 1;

                int total_linear = count_linear_vars_in_subtree(tree, 0, var_occurrences);

                if (var_occurrences[var_idx] > 1)
                {
                    // Non-linear variable: fixed state
                    long long factor = 1;
                    for (int i = 0; i < total_linear; ++i)
                    {
                        factor *= num_states;
                    }
                    std::vector<long long> counts(num_states, 0);
                    counts[fixed_assignment[var_idx]] = factor;
                    return counts;
                }
                else
                {
                    // Linear variable: uniform
                    long long factor = 1;
                    for (int i = 0; i < total_linear - 1; ++i)
                    {
                        factor *= num_states;
                    }
                    return std::vector<long long>(num_states, factor);
                }
            }
            else
            {
                int state = dfa.get_state_zero_arity(tree.nodes[node].symbol.m_name);
                int total_linear = count_linear_vars_in_subtree(tree, 0, var_occurrences);
                long long factor = 1;
                for (int i = 0; i < total_linear; ++i)
                {
                    factor *= num_states;
                }
                std::vector<long long> counts(num_states, 0);
                if (state >= 0 && state < num_states)
                {
                    counts[state] = factor;
                }
                return counts;
            }
        }

        // Internal node
        auto& children = tree.nodes[node].childs;
        int arity = static_cast<int>(children.size());

        std::vector<std::vector<long long>> child_dists(arity);
        for (int i = 0; i < arity; ++i)
        {
            child_dists[i] = count_node_recursive_fixed(tree, dfa, children[i], var_occurrences, fixed_assignment);
        }

        std::vector<long long> result(num_states, 0);
        std::vector<int> child_state_tuple(arity, 0);

        auto combine = [&](auto&& self, int pos) -> void
        {
            if (pos == arity)
            {
                long long product = 1;
                for (int i = 0; i < arity; ++i)
                {
                    product *= child_dists[i][child_state_tuple[i]];
                    if (product == 0) return;
                }
                auto parent_state = dfa.get_state(child_state_tuple, tree.nodes[node].symbol);
                if (parent_state.has_value() && parent_state.value() >= 0 && parent_state.value() < num_states)
                {
                    result[parent_state.value()] += product;
                }
                return;
            }
            for (int s = 0; s < num_states; ++s)
            {
                child_state_tuple[pos] = s;
                self(self, pos + 1);
            }
        };
        combine(combine, 0);

        return result;
    }

public:


    std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent(
          bottom_up_dtfta::BottomUpDTFA&          dfa,
          const std::vector<int>&                  state_to_tree_idx,
          const bit_matrix::BitMatrixColMajor&     membership_table,
          const std::vector<tree::ContextDAGTree>& contexts)
    {
        std::vector<CounterexampleCore> ctx_cores = get_ctx_cores(dfa);

        if (ctx_cores.empty())
        {
            return std::nullopt;
        }

        for (auto& ctx_core : ctx_cores)
        {
            const auto state_rep_idx1 = state_to_tree_idx[ctx_core.q1];
            const auto state_rep_idx2 = state_to_tree_idx[ctx_core.q2];
            const auto context_idx    = membership_table.column_first_differ_index(state_rep_idx1, state_rep_idx2);

            const tree::ContextDAGTree& context = contexts[context_idx];

            auto res = return_counterexample(ctx_core.tuple, {}, dfa);
            if (res.first.has_value() && res.second.has_value())
            {
                return std::make_pair(context.apply_context(res.first.value()),
                                      context.apply_context(res.second.value()));
            }
        }

        return std::nullopt;
    }

    /// Random consistency check: only checks num_samples random tuples.
    std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent_random(
          bottom_up_dtfta::BottomUpDTFA&          dfa,
          const std::vector<int>&                  state_to_tree_idx,
          const bit_matrix::BitMatrixColMajor&     membership_table,
          const std::vector<tree::ContextDAGTree>& contexts,
          int                                      num_samples,
          std::mt19937&                            gen)
    {
        std::vector<CounterexampleCore> ctx_cores = get_ctx_cores_random(dfa, num_samples, gen);

        if (ctx_cores.empty())
        {
            return std::nullopt;
        }

        for (auto& ctx_core : ctx_cores)
        {
            const auto state_rep_idx1 = state_to_tree_idx[ctx_core.q1];
            const auto state_rep_idx2 = state_to_tree_idx[ctx_core.q2];
            const auto context_idx    = membership_table.column_first_differ_index(state_rep_idx1, state_rep_idx2);

            const tree::ContextDAGTree& context = contexts[context_idx];

            auto res = return_counterexample(ctx_core.tuple, {}, dfa);
            if (res.first.has_value() && res.second.has_value())
            {
                return std::make_pair(context.apply_context(res.first.value()),
                                      context.apply_context(res.second.value()));
            }
        }

        return std::nullopt;
    }

    /// Counting heuristic: first counts tuples per state for left vs right.
    /// If distributions differ, enumerates all tuples to find the actual counterexample.
    /// If distributions are the same, returns nullopt (consistent).
    std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent_counting(
          bottom_up_dtfta::BottomUpDTFA&          dfa,
          const std::vector<int>&                  state_to_tree_idx,
          const bit_matrix::BitMatrixColMajor&     membership_table,
          const std::vector<tree::ContextDAGTree>& contexts)
    {
        // First: cheap counting check
        if (!has_counting_difference(dfa))
        {
            return std::nullopt;
        }

        // Distributions differ => enumerate all tuples to find actual counterexample
        return is_consistent(dfa, state_to_tree_idx, membership_table, contexts);
    }

    // not used
    /*std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent(bottom_up_dtfta::BottomUpDTFA& dfa)
    {
        std::vector<CounterexampleCore> ctx_cores = get_ctx_cores(dfa);

        if (ctx_cores.empty())
        {
            return std::nullopt;
        }

        auto get_index = [&](int i, int j)
        {
            if (j > i)
            {
                return j * dfa.get_states() + i;
            }
            return i * dfa.get_states() + j;
        };

        struct GraphNode
        {
            bottom_up_dtfta::Transition t1;
            bottom_up_dtfta::Transition t2;
        };


        std::vector<std::optional<GraphNode>> column_temp(dfa.get_states() * dfa.get_states(), std::nullopt);
        std::vector<std::vector<std::optional<GraphNode>>> graph(dfa.get_states() * dfa.get_states(), column_temp);

        for (auto& symbol : dfa.get_alphabet().get_symbols())
        {
            for (auto& t1 : dfa.get_delta_function(symbol.m_name))
            {
                for (auto& t2 : dfa.get_delta_function(symbol.m_name))
                {
                    std::optional<size_t> incoming_differs_index;
                    bool                  diff_more = false;
                    for (size_t i = 0; i < t1.m_incoming_states.size(); i++)
                    {
                        if (t1.m_incoming_states[i] == t2.m_incoming_states[i])
                        {
                            continue;
                        }

                        if (incoming_differs_index.has_value())
                        {
                            diff_more = true;
                            break;
                        }

                        incoming_differs_index = i;
                    }

                    if (diff_more || !incoming_differs_index.has_value())
                    {
                        continue;
                    }

                    const auto idx = get_index(t1.m_incoming_states[incoming_differs_index.value()],
                                               t2.m_incoming_states[incoming_differs_index.value()]);

                    const auto idx2 = get_index(t1.m_outgoing_state, t2.m_outgoing_state);


                    if (graph[idx][idx2].has_value())
                    {
                        if (graph[idx][idx2].value().t1.m_incoming_states.size() > t1.m_incoming_states.size())
                        {
                            graph[idx][idx2] = GraphNode { .t1 = t1, .t2 = t2 };
                        }
                    }
                    else
                    {
                        graph[idx][idx2] = GraphNode { .t1 = t1, .t2 = t2 };
                    }
                }
            }
        }

        for (auto& ctx_core : ctx_cores)
        {
            if (dfa.is_accepted_state(ctx_core.q1) != dfa.is_accepted_state(ctx_core.q2))
            {
                auto res = return_counterexample(ctx_core.tuple, {}, dfa);
                if (res.first.has_value() && res.second.has_value())
                {
                    return std::make_pair(res.first.value(), res.second.value());
                }
            }
        }

        for (auto& ctx_core : ctx_cores)
        {
            std::queue<std::vector<std::pair<bottom_up_dtfta::Transition, bottom_up_dtfta::Transition>>> q;

            auto idx = get_index(ctx_core.q1, ctx_core.q2);

            for (auto& neighbours : graph[idx])
            {
                if (neighbours.has_value())
                {
                    q.push({ std::make_pair(neighbours.value().t1, neighbours.value().t2) });
                }
            }

            while (!q.empty())
            {
                auto path = q.front();
                q.pop();

                auto curr_state1 = path.back().first.m_outgoing_state;
                auto curr_state2 = path.back().second.m_outgoing_state;

                if (dfa.is_accepted_state(curr_state1) != dfa.is_accepted_state(curr_state2))
                {
                    auto res = return_counterexample(ctx_core.tuple, path, dfa);
                    if (res.first.has_value() && res.second.has_value())
                    {
                        return std::make_pair(res.first.value(), res.second.value());
                    }
                }

                auto curr_idx = get_index(curr_state1, curr_state2);

                for (auto& neighbours : graph[curr_idx])
                {
                    if (!neighbours.has_value())
                    {
                        continue;
                    }

                    auto new_path = path;
                    auto step     = std::make_pair(neighbours.value().t1, neighbours.value().t2);

                    bool path_contains_step = false;
                    for (auto& p : new_path)
                    {
                        if (p.first == step.first && p.second == step.second)
                        {
                            path_contains_step = true;
                            break;
                        }
                    }

                    if (path_contains_step)
                    {
                        continue;
                    }

                    new_path.emplace_back(step);
                    q.push(new_path);
                }
            }
        }

        return std::nullopt;
    }*/
};

class TermRewritingSystem
{
    std::vector<TermRule> m_rules;

public:
    void add_rule(TermRule r)
    {
        m_rules.emplace_back(r);
    }

    std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent(
          bottom_up_dtfta::BottomUpDTFA&          dfa,
          const std::vector<int>&                  state_to_tree_idx,
          const bit_matrix::BitMatrixColMajor&     membership_table,
          const std::vector<tree::ContextDAGTree>& contexts)
    {
        for (auto& rule : m_rules)
        {
            auto result = rule.is_consistent(dfa, state_to_tree_idx, membership_table, contexts);
            if (result.has_value())
            {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Random consistency check: checks num_samples random tuples per rule.
    std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent_random(
          bottom_up_dtfta::BottomUpDTFA&          dfa,
          const std::vector<int>&                  state_to_tree_idx,
          const bit_matrix::BitMatrixColMajor&     membership_table,
          const std::vector<tree::ContextDAGTree>& contexts,
          int                                      num_samples,
          std::mt19937&                            gen)
    {
        for (auto& rule : m_rules)
        {
            auto result = rule.is_consistent_random(dfa, state_to_tree_idx, membership_table, contexts, num_samples, gen);
            if (result.has_value())
            {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Counting heuristic: for each rule, counts tuples per state for left vs right.
    /// If distributions differ, enumerates all tuples to find the counterexample.
    std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent_counting(
          bottom_up_dtfta::BottomUpDTFA&          dfa,
          const std::vector<int>&                  state_to_tree_idx,
          const bit_matrix::BitMatrixColMajor&     membership_table,
          const std::vector<tree::ContextDAGTree>& contexts)
    {
        for (auto& rule : m_rules)
        {
            auto result = rule.is_consistent_counting(dfa, state_to_tree_idx, membership_table, contexts);
            if (result.has_value())
            {
                return result;
            }
        }

        return std::nullopt;
    }


    //std::optional<std::pair<tree::DAGTree, tree::DAGTree>> is_consistent_On4(bottom_up_dtfta::BottomUpDTFA& dfa)
    //{
    //    //for (auto& rule : m_rules)
    //    //{
    //    //    auto result = rule.is_consistent(dfa);
    //    //    if (result.has_value())
    //    //    {
    //    //        return result.value();
    //    //    }
    //    //}

    //    auto get_index = [&](int i, int j)
    //    {
    //        //if (j > i)
    //        //{
    //        //    return j * dfa.get_states() + i;
    //        //}
    //        return i * dfa.get_states() + j;
    //    };

    //    struct GraphNode
    //    {
    //        tree::DAGTree t1;
    //        tree::DAGTree t2;
    //    };

    //    std::vector<std::optional<GraphNode>> graph(dfa.get_states() * dfa.get_states(), std::nullopt);

    //    for (int i = 0; i < dfa.get_states(); i++)
    //    {
    //        const auto st = dfa.get_small_tree(i);
    //        if (st.has_value())
    //        {
    //            graph[get_index(i, i)] = GraphNode { .t1 = st.value(), .t2 = st.value() };
    //        }
    //    }

    //    bool any_ctx_core = false;

    //    for (auto& rule : m_rules)
    //    {
    //        for (auto& ctx_core : rule.get_ctx_cores(dfa))
    //        {
    //            any_ctx_core   = true;
    //            auto index     = get_index(ctx_core.q1, ctx_core.q2);
    //            auto index_rev = get_index(ctx_core.q2, ctx_core.q1);
    //            if (graph[index].has_value())
    //            {
    //                continue;
    //            }

    //            std::vector<tree::DAGTree> childs;
    //            bool                       child_invalid = false;
    //            for (auto state : ctx_core.tuple)
    //            {
    //                auto child_index = get_index(state, state);
    //                if (!graph[child_index].has_value())
    //                {
    //                    child_invalid = true;
    //                    break;
    //                }

    //                childs.emplace_back(graph[child_index].value().t1);
    //            }
    //            if (child_invalid)
    //            {
    //                continue;
    //            }

    //            graph[index]     = GraphNode { .t1 = rule.get_left_tree(childs), .t2 = rule.get_right_tree(childs) };
    //            graph[index_rev] = GraphNode { .t1 = graph[index].value().t2, .t2 = graph[index].value().t1 };

    //            if (dfa.is_accepted_state(ctx_core.q1) != dfa.is_accepted_state(ctx_core.q2))
    //            {
    //                return std::make_pair(graph[index].value().t1, graph[index].value().t2);
    //            }
    //        }
    //    }

    //    if (!any_ctx_core)
    //    {
    //        return std::nullopt;
    //    }

    //    std::vector<std::pair<bottom_up_dtfta::Transition, bottom_up_dtfta::Transition>> transitions_to_analyze;
    //    for (auto& symbol : dfa.get_alphabet().get_symbols())
    //    {
    //        for (auto& t1 : dfa.get_delta_function(symbol.m_name))
    //        {
    //            for (auto& t2 : dfa.get_delta_function(symbol.m_name))
    //            {
    //                if (t1.m_outgoing_state != t2.m_outgoing_state)
    //                {
    //                    transitions_to_analyze.emplace_back(t1, t2);
    //                }
    //            }
    //        }
    //    }

    //    bool was_improved = true;
    //    while (was_improved)
    //    {
    //        was_improved = false;

    //        for (auto& [t1, t2] : transitions_to_analyze)
    //        {
    //            auto index     = get_index(t1.m_outgoing_state, t2.m_outgoing_state);
    //            auto index_rev = get_index(t2.m_outgoing_state, t1.m_outgoing_state);
    //            if (graph[index].has_value())
    //            {
    //                continue;
    //            }

    //            bool childs_exists = true;
    //            for (size_t i = 0; i < t1.m_incoming_states.size(); i++)
    //            {
    //                if (!graph[get_index(t1.m_incoming_states[i], t2.m_incoming_states[i])].has_value())
    //                {
    //                    childs_exists = false;
    //                    break;
    //                }
    //            }

    //            if (!childs_exists)
    //            {
    //                continue;
    //            }


    //            tree::DAGTree tree_left(dfa.get_alphabet());
    //            tree::DAGTree tree_right(dfa.get_alphabet());

    //            tree_left.add_node({}, t1.m_symbol);
    //            tree_right.add_node({}, t1.m_symbol);

    //            for (size_t i = 0; i < t1.m_incoming_states.size(); i++)
    //            {
    //                auto [child_left,
    //                      child_right] = graph[get_index(t1.m_incoming_states[i], t2.m_incoming_states[i])].value();

    //                tree_left.join_tree({ 0 }, child_left);
    //                tree_right.join_tree({ 0 }, child_right);
    //            }

    //            //tree_left.dagify_bottom_up();
    //            //tree_right.dagify_bottom_up();

    //            graph[index]     = { .t1 = tree_left, .t2 = tree_right };
    //            graph[index_rev] = { .t1 = tree_right, .t2 = tree_left };

    //            was_improved = true;

    //            if (dfa.is_accepted_state(t1.m_outgoing_state) != dfa.is_accepted_state(t2.m_outgoing_state))
    //            {
    //                tree_left.dagify_bottom_up();
    //                tree_right.dagify_bottom_up();
    //                return std::make_pair(tree_left, tree_right);
    //            }
    //        }

    //        auto erased_number = erase_if(
    //              transitions_to_analyze,
    //              [&](std::pair<bottom_up_dtfta::Transition, bottom_up_dtfta::Transition>& p)
    //              { return graph[get_index(p.first.m_outgoing_state, p.second.m_outgoing_state)].has_value(); });
    //    }


    //    return std::nullopt;
    //}
};
