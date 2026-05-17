#pragma once
#include <numeric>
#include <random>
#include <ranges>

#include "BitMatrix.h"
#include "BottomUpDTFA.h"
#include "Counterexample.h"
#include "ContextDAGTree.h"
#include "TermRewritingSystem.h"

namespace lstar {

class Teacher
{
protected:
    bottom_up_dtfta::BottomUpDTFA automata;

    int number_of_membership_queries  = 0;
    int number_of_equivalence_queries = 0;

public:
    Teacher(const bottom_up_dtfta::BottomUpDTFA& b)
          : automata(b)
    {}

    virtual ~Teacher() = default;

    virtual bool query_membership(const tree::Tree& t)
    {
        number_of_membership_queries = number_of_membership_queries + 1;
        return automata.is_accepted(t);
    }

    virtual bool query_membership(const tree::DAGTree& t)
    {
        number_of_membership_queries = number_of_membership_queries + 1;
        return automata.is_accepted(t);
    }

    virtual std::optional<tree::DAGTree> query_equivalence(const bottom_up_dtfta::BottomUpDTFA& candidate)
    {
        number_of_equivalence_queries = number_of_equivalence_queries + 1;
        auto result                   = counterexample::find_counterexample(automata, candidate);


        //std::cout << "Candidate automata:\n" << candidate.to_string() << "\n\n";

        if (result.has_value())
        {
            //std::cout << "Counterexample: \n" << result.value().to_string() << "\n\n";
            return result;
        }

        //std::cout << "Result automata:\n" << candidate.to_string();
        return result;
    }

    virtual std::string get_notes()
    {
        return std::format("Membership queries: {}, Equivalence queries: {}, Number of states in pattern automata: {}\n",
                           number_of_membership_queries,
                           number_of_equivalence_queries,
                           automata.get_states());
    }

    int get_number_eq() const
    {
        return number_of_equivalence_queries;
    }

    int get_number_mem() const
    {
        return number_of_membership_queries;
    }

    int get_result_states() const
    {
        return automata.get_states();
    }
};

struct pair_hash
{
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const
    {
        auto h1 = std::hash<T1> {}(p.first);
        auto h2 = std::hash<T2> {}(p.second);

        // Mainly for demonstration purposes, i.e. works but is overly simple
        // In the real world, use sth. like boost.hash_combine
        return h1 ^ h2;
    }
};


class Student
{
public:
    using Symbol = alphabet::Symbol;

    size_t last_i = 0;
    size_t last_j = 0;

    std::vector<tree::DAGTree>        state_trees;
    std::vector<tree::ContextDAGTree> contexts;

    struct StateIndices
    {
        std::unordered_map<std::vector<uint64_t>, std::vector<int>> indices;
        int                                                         last_st = 0;
        int                                                         last_ct = 0;
    } m_state_indices;

    StateIndices get_updated_indices()
    {
        StateIndices res;

        for (int i = m_state_indices.last_st; i < state_trees.size(); i++)
        {
            auto vec = membership_table2.get_column(i);

            // Truncate to only the bits covering contexts [0, last_ct),
            // so the j-loop below can properly split by new contexts.
            size_t truncated_blocks = (m_state_indices.last_ct + 63) / 64;
            if (vec.size() > truncated_blocks)
            {
                vec.resize(truncated_blocks);
            }
            // Clear any trailing bits beyond last_ct within the last block
            if (truncated_blocks > 0 && m_state_indices.last_ct % 64 != 0)
            {
                uint64_t mask = (uint64_t{1} << (m_state_indices.last_ct % 64)) - 1;
                vec[truncated_blocks - 1] &= mask;
            }

            if (!m_state_indices.indices.contains(vec))
            {
                m_state_indices.indices[vec] = {};
            }
            m_state_indices.indices[vec].emplace_back(i);
        }

        for (int j = m_state_indices.last_ct; j < contexts.size(); j++)
        {
            // Each iteration splits the current partition by context j.
            // We must read from the result of the previous split, not the original.
            std::unordered_map<std::vector<uint64_t>, std::vector<int>> next;

            auto& source = (j == m_state_indices.last_ct) ? m_state_indices.indices : res.indices;

            for (auto& p : source)
            {
                std::vector<int> t;
                std::vector<int> f;

                for (auto idx : p.second)
                {
                    if (membership_table2.get(j, idx))
                    {
                        t.emplace_back(idx);
                    }
                    else
                    {
                        f.emplace_back(idx);
                    }
                }

                std::vector<uint64_t> vt = p.first;
                std::vector<uint64_t> vf = p.first;

                if (j % 64 == 0)
                {
                    vt.emplace_back(1);
                    vf.emplace_back(0);
                }
                else
                {
                    auto mask   = (uint64_t { 1 } << (j % 64));
                    vt[j / 64] |= mask;
                    vf[j / 64] &= ~mask;
                }

                if (!t.empty())
                {
                    next[vt] = t;
                }

                if (!f.empty())
                {
                    next[vf] = f;
                }
            }

            res.indices = std::move(next);
        }

        if (m_state_indices.last_ct == contexts.size())
        {
            res.indices = m_state_indices.indices;
        }

        res.last_st = state_trees.size();
        res.last_ct = contexts.size();

        m_state_indices = res;
        return m_state_indices;
    }

    std::unordered_map<size_t, size_t> tree_hash_to_idx;
    std::unordered_set<size_t>         ct_hashes;

    std::unordered_set<int> old_indices = {};

    bit_matrix::BitMatrixColMajor membership_table2;

    bottom_up_dtfta::BottomUpDTFA hypothesis;
    std::vector<int>               hypothesis_states_to_tree_idx;

    Teacher&           teacher;
    alphabet::Alphabet alphabet;

    Student(Teacher& t, const alphabet::Alphabet& a)
          : hypothesis(a, 0, {}),
            teacher(t),
            alphabet(a)
    {}

    // Initialize observation table with the empty tree/context
    void initialize()
    {
        state_trees.clear();
        contexts.clear();
        tree_hash_to_idx = {};
        ct_hashes        = {};
        membership_table2.clear();

        m_state_indices = {};

        // Add minimal tree(s) to state_trees and contexts
        tree::DAGTree        empty_tree(alphabet);
        tree::ContextDAGTree empty_ctx = tree::ContextDAGTree::get_empty_context(alphabet);


        //state_trees.push_back(empty_tree);
        contexts.push_back(empty_ctx);
        ct_hashes.insert(contexts[0].tree_hash());

        // Fill initial membership table
        update_membership();
    }


    void update_membership()
    {
        membership_table2.resize(contexts.size(), state_trees.size());
        for (size_t i = last_i; i < state_trees.size(); ++i)
        {
            for (size_t j = 0; j < contexts.size(); ++j)
            {
                tree::DAGTree combined = contexts[j].apply_context(state_trees[i]);
                bool          member   = teacher.query_membership(combined);
                membership_table2.set(j, i, member);
            }
        }

        for (size_t i = 0; i < last_i; ++i)
        {
            for (size_t j = last_j; j < contexts.size(); ++j)
            {
                tree::DAGTree combined = contexts[j].apply_context(state_trees[i]);
                bool          member   = teacher.query_membership(combined);
                membership_table2.set(j, i, member);
            }
        }

        last_i = state_trees.size();
        last_j = contexts.size();
    }


    void build_hypothesis()
    {
        hypothesis_states_to_tree_idx.clear();
        for (auto& p : m_state_indices.indices | std::views::values)
        {
            hypothesis_states_to_tree_idx.emplace_back(p[0]);
        }
        std::ranges::sort(hypothesis_states_to_tree_idx);
        std::unordered_map<int, int> treeidx_to_state;
        for (int i = 0; i < hypothesis_states_to_tree_idx.size(); i++)
        {
            treeidx_to_state[hypothesis_states_to_tree_idx[i]] = i;
        }

        hypothesis = bottom_up_dtfta::BottomUpDTFA(alphabet, m_state_indices.indices.size(), {});

        for (const auto& sym : alphabet.get_symbols())
        {
            std::vector<int> child_state_tuple(sym.m_arity);

            auto gen = [&](auto&& self, size_t pos) -> void
            {
                if (pos == sym.m_arity)
                {
                    // Build the tree for this combination
                    tree::DAGTree tree(alphabet);
                    tree.add_node({}, sym);
                    for (auto idx : child_state_tuple)
                    {
                        tree.join_tree({ 0 }, state_trees[idx]);
                    }
                    tree.dagify_bottom_up();
                    size_t i   = tree_hash_to_idx[tree.tree_hash()];
                    auto   vec = membership_table2.get_column(i);

                    int              parent_state = treeidx_to_state[m_state_indices.indices[vec][0]];
                    std::vector<int> proper_child_states(child_state_tuple.size(), 0);
                    for (int i = 0; i < child_state_tuple.size(); i++)
                    {
                        proper_child_states[i] = treeidx_to_state[child_state_tuple[i]];
                    }

                    hypothesis.add_transition(bottom_up_dtfta::Transition { .m_incoming_states = proper_child_states,
                                                                            .m_symbol          = sym.m_name,
                                                                            .m_outgoing_state  = parent_state });
                    return;
                }

                for (auto& val : m_state_indices.indices | std::views::values)
                {
                    child_state_tuple[pos] = val[0];
                    self(self, pos + 1);
                }
            };
            gen(gen, 0);
        }

        for (auto& p : m_state_indices.indices)
        {
            if (p.first[0] & uint64_t { 1 })
            {
                hypothesis.set_final_state(treeidx_to_state[p.second[0]]);
            }
        }
    }


    bool compute_closure()
    {
        bool closed = true;

        get_updated_indices();

        for (const auto& sym : alphabet.get_symbols())
        {
            auto gen = [&](auto&& self, std::vector<size_t>& tuple, size_t pos, size_t pos_new) -> void
            {
                if (pos != sym.m_arity)
                {
                    for (auto& val : m_state_indices.indices | std::views::values)
                    {
                        auto indice = val[0];
                        if (pos == pos_new && old_indices.contains(indice))
                        {
                            continue;
                        }
                        tuple[pos] = indice;
                        self(self, tuple, pos + 1, pos_new);
                    }
                    return;
                }

                tree::DAGTree new_tree(alphabet);
                new_tree.add_node({}, sym);

                for (size_t idx : tuple)
                {
                    new_tree.join_tree({ 0 }, state_trees[idx]);
                }
                new_tree.dagify_bottom_up();

                bool already_present = tree_hash_to_idx.contains(new_tree.tree_hash());
                if (!already_present)
                {
                    state_trees.push_back(new_tree);
                    tree_hash_to_idx[new_tree.tree_hash()] = state_trees.size() - 1;
                    closed                                 = false;
                }
            };

            std::vector<size_t> tuple(sym.m_arity);
            for (int i = 0; i < sym.m_arity; i++)
            {
                gen(gen, tuple, 0, i);
            }
            if (sym.m_arity == 0)
            {
                gen(gen, tuple, 0, 1);
            }
        }

        if (!closed)
        {
            update_membership();
        }

        for (auto& val : m_state_indices.indices | std::views::values)
        {
            if (!old_indices.contains(val[0]))
            {
                old_indices.insert(val[0]);
            }
        }

        return closed;
    }


    bool add_one_state_with_counterexample(const tree::DAGTree& counterexample)
    {
        auto subtree = counterexample;
        for (auto& node : subtree.nodes)
        {
            tree::DAGTree tree = subtree.extract_subtree(node.id);

            auto indices_backup = get_updated_indices();

            bool tree_added = !tree_hash_to_idx.contains(tree.tree_hash());
            if (tree_added)
            {
                state_trees.emplace_back(tree);
                tree_hash_to_idx[tree.tree_hash()] = state_trees.size() - 1;
                update_membership();
            }

            for (auto& nc : tree::ContextDAGTree::make_context(subtree, node.id))
            {
                auto indices_backup2 = get_updated_indices();
                bool context_added   = !ct_hashes.contains(nc.tree_hash());

                if (!context_added && m_state_indices.indices.size() <= old_indices.size())
                {
                    continue;
                }

                if (context_added)
                {
                    contexts.emplace_back(nc);
                    ct_hashes.insert(nc.tree_hash());
                    update_membership();
                }

                get_updated_indices();

                if (m_state_indices.indices.size() > old_indices.size())
                {
                    return true;
                }

                if (context_added)
                {
                    membership_table2.resize(contexts.size() - 1, state_trees.size());
                    last_j--;
                    ct_hashes.erase(contexts.back().tree_hash());
                    contexts.erase(std::prev(contexts.end()));
                    m_state_indices = indices_backup2;
                }
            }

            if (tree_added)
            {
                membership_table2.resize(contexts.size(), state_trees.size() - 1);
                last_i--;
                tree_hash_to_idx.erase(state_trees.back().tree_hash());
                state_trees.erase(std::prev(state_trees.end()));
                m_state_indices = indices_backup;
            }
        }

        auto pattern_automata_res = teacher.query_membership(counterexample);
        auto my_automata_res      = hypothesis.is_accepted(counterexample);

        return false;
        std::string mess = "Failed while trying to expand hypothesis with counterexample\n";
        std::cerr << mess;
        throw std::invalid_argument(mess);
    }


    // Main learning loop
    virtual std::optional<bottom_up_dtfta::BottomUpDTFA> learn()
    {
        initialize();

        while (true)
        {
            auto start_cc = std::chrono::high_resolution_clock::now();
            // Ensure table is closed and consistent
            while (!compute_closure())
            {
            }
            auto stop_cc    = std::chrono::high_resolution_clock::now();
            auto elapsed_cc = std::chrono::duration_cast<std::chrono::seconds>(stop_cc - start_cc);
            if (elapsed_cc.count() > 5)
            {
                std::cout << "Compute Closure " << elapsed_cc << "\n";
            }

            auto start_bh = std::chrono::high_resolution_clock::now();
            build_hypothesis();
            auto stop_bh    = std::chrono::high_resolution_clock::now();
            auto elapsed_bh = std::chrono::duration_cast<std::chrono::seconds>(stop_bh - start_bh);
            if (elapsed_bh.count() > 5)
            {
                std::cout << "Build Hypothesis " << elapsed_bh << "\n";
            }

            // Equivalence query
            auto start_ctx      = std::chrono::high_resolution_clock::now();
            auto counterexample = teacher.query_equivalence(hypothesis);
            auto stop_ctx       = std::chrono::high_resolution_clock::now();
            auto elapsed_ctx    = std::chrono::duration_cast<std::chrono::seconds>(stop_ctx - start_ctx);
            if (elapsed_ctx.count() > 5)
            {
                std::cout << "Build Counterexample " << elapsed_ctx << "\n";
            }

            if (!counterexample.has_value())
            {
                return hypothesis;
            }

            // Refine table with counterexample
            //refine_with_counterexample(counterexample.value());
            auto start_ac           = std::chrono::high_resolution_clock::now();
            bool automata_is_proper = add_one_state_with_counterexample(counterexample.value());
            auto stop_ac            = std::chrono::high_resolution_clock::now();
            auto elapsed_ac         = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
            if (elapsed_ac.count() > 5)
            {
                std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
            }

            if (!automata_is_proper)
            {
                return std::nullopt;
            }
        }
    }
};


class StudentAdvice : public Student
{
protected:
    TermRewritingSystem advice;

public:
    StudentAdvice(Teacher& t, const alphabet::Alphabet& a, TermRewritingSystem& trs)
          : Student(t, a),
            advice(trs)
    {}

    std::optional<bottom_up_dtfta::BottomUpDTFA> learn() override
    {
        initialize();

        while (true)
        {
            // Ensure table is closed and consistent
            auto start_cc = std::chrono::high_resolution_clock::now();
            while (!compute_closure())
            {
            }
            auto stop_cc    = std::chrono::high_resolution_clock::now();
            auto elapsed_cc = std::chrono::duration_cast<std::chrono::seconds>(stop_cc - start_cc);
            if (elapsed_cc.count() > 5)
            {
                std::cout << "Compute Closure " << elapsed_cc << "\n";
            }

            auto start_bh = std::chrono::high_resolution_clock::now();
            build_hypothesis();
            auto stop_bh    = std::chrono::high_resolution_clock::now();
            auto elapsed_bh = std::chrono::duration_cast<std::chrono::seconds>(stop_bh - start_bh);
            if (elapsed_bh.count() > 5)
            {
                std::cout << "Build Hypothesis " << elapsed_bh << "\n";
            }

            auto start_ic     = std::chrono::high_resolution_clock::now();
            auto possible_ctx = advice.is_consistent(hypothesis, hypothesis_states_to_tree_idx, membership_table2, contexts);
            auto stop_ic      = std::chrono::high_resolution_clock::now();
            auto elapsed_ic   = std::chrono::duration_cast<std::chrono::seconds>(stop_ic - start_ic);
            if (elapsed_ic.count() > 5)
            {
                std::cout << "Is Consistent " << elapsed_ic << " Number of states " << hypothesis.get_states() << "\n";
            }


            if (possible_ctx.has_value())
            {
                auto start_ac    = std::chrono::high_resolution_clock::now();
                auto first_tree  = possible_ctx.value().first;
                auto second_tree = possible_ctx.value().second;

                bool is_accepted = teacher.query_membership(first_tree);
                //bool is_accepted2 = teacher.query_membership(second_tree);

                //auto is1 = hypothesis.is_accepted(first_tree);
                //auto is2 = hypothesis.is_accepted(second_tree);

                bool is_proper_automata = true;
                if (hypothesis.is_accepted(first_tree) != is_accepted)
                {
                    //refine_with_counterexample(first_tree);
                    is_proper_automata = add_one_state_with_counterexample(first_tree);
                }
                else
                {
                    //refine_with_counterexample(second_tree);
                    is_proper_automata = add_one_state_with_counterexample(second_tree);
                }

                if (!is_proper_automata)
                {
                    return std::nullopt;
                }

                auto stop_ac    = std::chrono::high_resolution_clock::now();
                auto elapsed_ac = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
                if (elapsed_ac.count() > 5)
                {
                    std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
                }
                continue;
            }

            // Equivalence query
            auto start_ctx      = std::chrono::high_resolution_clock::now();
            auto counterexample = teacher.query_equivalence(hypothesis);
            auto stop_ctx       = std::chrono::high_resolution_clock::now();
            auto elapsed_ctx    = std::chrono::duration_cast<std::chrono::seconds>(stop_ctx - start_ctx);
            if (elapsed_ctx.count() > 5)
            {
                std::cout << "Build Counterexample " << elapsed_ctx << "\n";
            }

            if (!counterexample.has_value())
            {
                return hypothesis;
            }

            // Refine table with counterexample
            //refine_with_counterexample(counterexample.value());
            auto start_ac           = std::chrono::high_resolution_clock::now();
            bool is_proper_automata = add_one_state_with_counterexample(counterexample.value());
            auto stop_ac            = std::chrono::high_resolution_clock::now();
            auto elapsed_ac         = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
            if (elapsed_ac.count() > 5)
            {
                std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
            }

            if (!is_proper_automata)
            {
                return std::nullopt;
            }
        }
    }
};

/// StudentAdviceRandomCheck: Like StudentAdvice, but instead of checking ALL
/// combinations of state tuples for consistency, it checks a random subset.
/// The number of random tuples checked per rule is |Q| (the number of states
/// in the current hypothesis).
class StudentAdviceRandomCheck : public StudentAdvice
{
    std::mt19937                   m_gen;

public:
    StudentAdviceRandomCheck(Teacher& t, const alphabet::Alphabet& a, TermRewritingSystem& trs,
                             std::optional<unsigned int> seed = std::nullopt)
          : StudentAdvice(t, a, trs)
    {
        if (seed.has_value())
        {
            m_gen.seed(seed.value());
        }
        else
        {
            std::random_device rd;
            m_gen.seed(rd());
        }
    }

    std::optional<bottom_up_dtfta::BottomUpDTFA> learn() override
    {
        initialize();

        while (true)
        {
            // Ensure table is closed and consistent
            auto start_cc = std::chrono::high_resolution_clock::now();
            while (!compute_closure())
            {
            }
            auto stop_cc    = std::chrono::high_resolution_clock::now();
            auto elapsed_cc = std::chrono::duration_cast<std::chrono::seconds>(stop_cc - start_cc);
            if (elapsed_cc.count() > 5)
            {
                std::cout << "Compute Closure " << elapsed_cc << "\n";
            }

            auto start_bh = std::chrono::high_resolution_clock::now();
            build_hypothesis();
            auto stop_bh    = std::chrono::high_resolution_clock::now();
            auto elapsed_bh = std::chrono::duration_cast<std::chrono::seconds>(stop_bh - start_bh);
            if (elapsed_bh.count() > 5)
            {
                std::cout << "Build Hypothesis " << elapsed_bh << "\n";
            }

            // Random consistency check: sample |Q| random tuples
            auto start_ic     = std::chrono::high_resolution_clock::now();
            int  num_samples  = hypothesis.get_states();
            auto possible_ctx = advice.is_consistent_random(hypothesis, hypothesis_states_to_tree_idx,
                                                            membership_table2, contexts, num_samples, m_gen);
            auto stop_ic      = std::chrono::high_resolution_clock::now();
            auto elapsed_ic   = std::chrono::duration_cast<std::chrono::seconds>(stop_ic - start_ic);
            if (elapsed_ic.count() > 5)
            {
                std::cout << "Is Consistent (Random) " << elapsed_ic << " Number of states " << hypothesis.get_states() << "\n";
            }

            if (possible_ctx.has_value())
            {
                auto start_ac    = std::chrono::high_resolution_clock::now();
                auto first_tree  = possible_ctx.value().first;
                auto second_tree = possible_ctx.value().second;

                bool is_accepted = teacher.query_membership(first_tree);

                bool is_proper_automata = true;
                if (hypothesis.is_accepted(first_tree) != is_accepted)
                {
                    is_proper_automata = add_one_state_with_counterexample(first_tree);
                }
                else
                {
                    is_proper_automata = add_one_state_with_counterexample(second_tree);
                }

                if (!is_proper_automata)
                {
                    return std::nullopt;
                }

                auto stop_ac    = std::chrono::high_resolution_clock::now();
                auto elapsed_ac = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
                if (elapsed_ac.count() > 5)
                {
                    std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
                }
                continue;
            }

            // Equivalence query
            auto start_ctx      = std::chrono::high_resolution_clock::now();
            auto counterexample = teacher.query_equivalence(hypothesis);
            auto stop_ctx       = std::chrono::high_resolution_clock::now();
            auto elapsed_ctx    = std::chrono::duration_cast<std::chrono::seconds>(stop_ctx - start_ctx);
            if (elapsed_ctx.count() > 5)
            {
                std::cout << "Build Counterexample " << elapsed_ctx << "\n";
            }

            if (!counterexample.has_value())
            {
                return hypothesis;
            }

            auto start_ac           = std::chrono::high_resolution_clock::now();
            bool is_proper_automata = add_one_state_with_counterexample(counterexample.value());
            auto stop_ac            = std::chrono::high_resolution_clock::now();
            auto elapsed_ac         = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
            if (elapsed_ac.count() > 5)
            {
                std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
            }

            if (!is_proper_automata)
            {
                return std::nullopt;
            }
        }
    }
};


/// StudentAdviceCountingHeuristic: Uses a counting heuristic to detect inconsistency.
/// "Heurystyka zliczania ile krotek prowadzi do każdego stanu dla reguły.
///  Jeśli jest różnica to przeglądamy wszystkie krotki."
///
/// For each rule, counts how many tuples lead to each state for the left side
/// vs the right side. If the distributions differ, there must be an inconsistency,
/// so we then enumerate all tuples to find the actual counterexample.
/// This avoids full enumeration when the hypothesis is already consistent.
class StudentAdviceCountingHeuristic : public StudentAdvice
{
public:
    StudentAdviceCountingHeuristic(Teacher& t, const alphabet::Alphabet& a, TermRewritingSystem& trs)
          : StudentAdvice(t, a, trs)
    {}

    std::optional<bottom_up_dtfta::BottomUpDTFA> learn() override
    {
        initialize();

        while (true)
        {
            // Ensure table is closed and consistent
            auto start_cc = std::chrono::high_resolution_clock::now();
            while (!compute_closure())
            {
            }
            auto stop_cc    = std::chrono::high_resolution_clock::now();
            auto elapsed_cc = std::chrono::duration_cast<std::chrono::seconds>(stop_cc - start_cc);
            if (elapsed_cc.count() > 5)
            {
                std::cout << "Compute Closure " << elapsed_cc << "\n";
            }

            auto start_bh = std::chrono::high_resolution_clock::now();
            build_hypothesis();
            auto stop_bh    = std::chrono::high_resolution_clock::now();
            auto elapsed_bh = std::chrono::duration_cast<std::chrono::seconds>(stop_bh - start_bh);
            if (elapsed_bh.count() > 5)
            {
                std::cout << "Build Hypothesis " << elapsed_bh << "\n";
            }

            // Counting heuristic: first check if state distributions differ
            auto start_ic     = std::chrono::high_resolution_clock::now();
            auto possible_ctx = advice.is_consistent_counting(hypothesis, hypothesis_states_to_tree_idx,
                                                              membership_table2, contexts);
            auto stop_ic      = std::chrono::high_resolution_clock::now();
            auto elapsed_ic   = std::chrono::duration_cast<std::chrono::seconds>(stop_ic - start_ic);
            if (elapsed_ic.count() > 5)
            {
                std::cout << "Is Consistent (Counting) " << elapsed_ic << " Number of states " << hypothesis.get_states() << "\n";
            }

            if (possible_ctx.has_value())
            {
                auto start_ac    = std::chrono::high_resolution_clock::now();
                auto first_tree  = possible_ctx.value().first;
                auto second_tree = possible_ctx.value().second;

                bool is_accepted = teacher.query_membership(first_tree);

                bool is_proper_automata = true;
                if (hypothesis.is_accepted(first_tree) != is_accepted)
                {
                    is_proper_automata = add_one_state_with_counterexample(first_tree);
                }
                else
                {
                    is_proper_automata = add_one_state_with_counterexample(second_tree);
                }

                if (!is_proper_automata)
                {
                    return std::nullopt;
                }

                auto stop_ac    = std::chrono::high_resolution_clock::now();
                auto elapsed_ac = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
                if (elapsed_ac.count() > 5)
                {
                    std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
                }
                continue;
            }

            // Equivalence query
            auto start_ctx      = std::chrono::high_resolution_clock::now();
            auto counterexample = teacher.query_equivalence(hypothesis);
            auto stop_ctx       = std::chrono::high_resolution_clock::now();
            auto elapsed_ctx    = std::chrono::duration_cast<std::chrono::seconds>(stop_ctx - start_ctx);
            if (elapsed_ctx.count() > 5)
            {
                std::cout << "Build Counterexample " << elapsed_ctx << "\n";
            }

            if (!counterexample.has_value())
            {
                return hypothesis;
            }

            auto start_ac           = std::chrono::high_resolution_clock::now();
            bool is_proper_automata = add_one_state_with_counterexample(counterexample.value());
            auto stop_ac            = std::chrono::high_resolution_clock::now();
            auto elapsed_ac         = std::chrono::duration_cast<std::chrono::seconds>(stop_ac - start_ac);
            if (elapsed_ac.count() > 5)
            {
                std::cout << "Analyze Counterexample " << elapsed_ac << "\n";
            }

            if (!is_proper_automata)
            {
                return std::nullopt;
            }
        }
    }
};

} // namespace lstar
