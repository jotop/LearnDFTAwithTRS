#pragma once
#include <functional>
#include <optional>
#include <random>

#include "Lstar.h"


namespace lstar {

// Generates a random tree over the given alphabet up to max_depth.
inline tree::DAGTree generate_random_tree_for_teacher(const alphabet::Alphabet& alphabet,
                                                      int                       max_depth,
                                                      std::mt19937&             gen)
{
    tree::DAGTree tree(alphabet);

    const auto& symbols = alphabet.get_symbols();

    std::vector<const alphabet::Symbol*> nullary_symbols;
    for (const auto& sym : symbols)
    {
        if (sym.m_arity == 0)
        {
            nullary_symbols.push_back(&sym);
        }
    }

    std::function<void(std::vector<tree::NodeId>, int)> build = [&](std::vector<tree::NodeId> father, int depth)
    {
        if (depth <= 0)
        {
            std::uniform_int_distribution<size_t> leaf_dist(0, nullary_symbols.size() - 1);
            tree.add_node(father, *nullary_symbols[leaf_dist(gen)]);
            return;
        }

        std::uniform_int_distribution<int> choice(0, static_cast<int>(symbols.size()) - 1);
        const auto&                        sym = symbols[choice(gen)];

        auto node_id = tree.add_node(father, sym);
        for (size_t i = 0; i < sym.m_arity; i++)
        {
            build({ node_id }, depth - 1);
        }
    };

    build({}, max_depth);
    return tree;
}


/// TeacherApproximate: inherits from Teacher, overrides equivalence query
/// to use random sampling instead of exact counterexample search.
///
/// Also counts "tokens" in membership queries, where tokens = number of nodes
/// in the queried tree (i.e., the tree size).
class TeacherApproximate : public Teacher
{
    long long membership_tokens = 0; // total number of nodes across all membership queries

    int          m_num_random_tries; // number of random trees to test per equivalence query
    int          m_max_tree_depth;   // max depth of generated random trees
    std::mt19937 m_gen;

public:
    bool is_proper_automata_found = false;

    /// @param b              The target automaton (the one we are trying to learn)
    /// @param num_tries      Number of random trees to test per equivalence query
    /// @param max_tree_depth Maximum depth of randomly generated trees
    /// @param seed           Random seed (optional, defaults to random_device)
    TeacherApproximate(const bottom_up_dtfta::BottomUpDTFA& b,
                       int                                  num_tries,
                       int                                  max_tree_depth = 20,
                       std::optional<unsigned int>          seed           = std::nullopt)
          : Teacher(b),
            m_num_random_tries(num_tries),
            m_max_tree_depth(max_tree_depth)
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

    bool query_membership(const tree::Tree& t) override
    {
        membership_tokens += static_cast<long long>(t.nodes.size());
        return Teacher::query_membership(t);
    }

    bool query_membership(const tree::DAGTree& t) override
    {
        membership_tokens += static_cast<long long>(t.nodes.size());
        return Teacher::query_membership(t);
    }

    /// Approximate equivalence query: generates random trees and checks if
    /// the candidate automaton agrees with the target on all of them.
    /// Returns a counterexample (a tree where they disagree) or nullopt if
    /// no disagreement was found within the budget of random tries.
    std::optional<tree::DAGTree> query_equivalence(const bottom_up_dtfta::BottomUpDTFA& candidate) override
    {
        number_of_equivalence_queries++;

        const auto& alpha = automata.get_alphabet();

        for (int i = 0; i < m_num_random_tries; i++)
        {
            auto random_tree = generate_random_tree_for_teacher(alpha, m_max_tree_depth, m_gen);

            bool target_accepts    = automata.is_accepted(random_tree);
            bool candidate_accepts = candidate.is_accepted(random_tree);

            if (target_accepts != candidate_accepts)
            {
                return random_tree;
            }
        }

        if (!counterexample::find_counterexample(automata, candidate).has_value())
        {
            is_proper_automata_found = true;
        }
        // No counterexample found — assume equivalence
        return std::nullopt;
    }

    std::string get_notes() override
    {
        return std::format("Membership queries: {}, Equivalence queries: {}, Membership tokens (total nodes): {}, "
                           "Number of states in pattern automata: {}\n",
                           number_of_membership_queries,
                           number_of_equivalence_queries,
                           membership_tokens,
                           automata.get_states());
    }

    long long get_membership_tokens() const
    {
        return membership_tokens;
    }

    int get_num_random_tries() const
    {
        return m_num_random_tries;
    }

    void set_num_random_tries(int num_tries)
    {
        m_num_random_tries = num_tries;
    }

    int get_max_tree_depth() const
    {
        return m_max_tree_depth;
    }

    void set_max_tree_depth(int depth)
    {
        m_max_tree_depth = depth;
    }
};

} // namespace lstar
