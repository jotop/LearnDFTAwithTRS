#pragma once
#include "Alfabet.h"
#include "Tree.h"
#include "DAGTree.h"

namespace std {
template<>
struct hash<std::vector<int>>
{
    size_t operator()(const std::vector<int>& vec) const noexcept
    {
        size_t seed = 0;
        for (int i : vec)
        {
            // Combine the hash of each element into the seed
            seed ^= std::hash<int> {}(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

template<>
struct hash<std::vector<uint64_t>>
{
    size_t operator()(const std::vector<uint64_t>& vec) const noexcept
    {
        size_t seed = 0;
        for (uint64_t i : vec)
        {
            // Combine the hash of each element into the seed
            seed ^= std::hash<uint64_t> {}(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
}


//namespace std {
//template<>
//struct hash<std::vector<uint64_t>>
//{
//    size_t operator()(const std::vector<int>& vec) const noexcept
//    {
//        size_t seed = 0;
//        for (int i : vec)
//        {
//            // Combine the hash of each element into the seed
//            seed ^= std::hash<int> {}(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//        }
//        return seed;
//    }
//};
//}

namespace bottom_up_dtfta {

struct Transition
{
    std::vector<int> m_incoming_states;
    std::string      m_symbol;
    int              m_outgoing_state;

    bool operator==(const Transition& other) const = default;
};


class BottomUpDTFA
{
    alphabet::Alphabet m_alphabet;
    int                m_states = 0; // states as numbers from range 0, 1, m_states-1
    std::vector<int>   m_accepting_states = {};

    std::vector<Transition>                                                    m_delta_function = {};
    std::unordered_map<std::string, std::vector<Transition>>                   delta_by_symbol  = {};
    std::unordered_map<std::string, std::unordered_map<std::vector<int>, int>> efficient_delta  = {};


    std::vector<std::optional<tree::DAGTree>> small_trees = std::vector<std::optional<tree::DAGTree>>(m_states,
                                                                                                      std::nullopt);

public:
    std::optional<int> get_state(const std::vector<int>& childs_states, const alphabet::Symbol& symbol) const
    {
        auto sym_name = symbol.m_name;
        if (!efficient_delta.contains(sym_name))
        {
            return std::nullopt;
        }

        if (!efficient_delta.at(sym_name).contains(childs_states))
        {
            return std::nullopt;
        }

        return efficient_delta.at(sym_name).at(childs_states);
    }

    BottomUpDTFA(const alphabet::Alphabet& a, int states, const std::vector<int>& f)
          : m_alphabet(a),
            m_states(states),
            m_accepting_states(f)
    {}

    BottomUpDTFA(const alphabet::Alphabet& a)
          : m_alphabet(a)
    {}

    void set_states(int states)
    {
        m_states = states;
        small_trees.assign(m_states, std::nullopt);
    }


    std::string to_string() const
    {
        std::string result = std::format("Automata with {} states. Accepting states: ", m_states);

        for (auto acc : m_accepting_states)
        {
            result += std::format("{},", acc);
        }

        result += ". Transitions:\n";

        for (auto& t : m_delta_function)
        {
            std::string incoming;
            for (auto in : t.m_incoming_states)
            {
                incoming += std::format("{},", in);
            }
            result += std::format("IN: {} SYM: {}, OUT: {}\n", incoming, t.m_symbol, t.m_outgoing_state);
        }

        result += "\n";

        return result;
    }


    const alphabet::Alphabet& get_alphabet() const
    {
        return m_alphabet;
    }

    const std::vector<Transition>& get_delta_function() const
    {
        return m_delta_function;
    }

    const std::vector<Transition>& get_delta_function(const std::string& symbol) const
    {
        return delta_by_symbol.at(symbol);
    }

    int get_state_zero_arity(const std::string& sym_name) const
    {
        return efficient_delta.at(sym_name).at({});
    }

    const int& get_states() const
    {
        return m_states;
    }

    BottomUpDTFA& add_transition(const Transition& t)
    {
        if (!efficient_delta.contains(t.m_symbol))
        {
            efficient_delta[t.m_symbol] = {};
        }

        efficient_delta[t.m_symbol][t.m_incoming_states] = t.m_outgoing_state;

        if (!delta_by_symbol.contains(t.m_symbol))
        {
            delta_by_symbol[t.m_symbol] = {};
        }

        delta_by_symbol[t.m_symbol].emplace_back(t);

        m_delta_function.emplace_back(t);
        return *this;
    }

    bool is_accepted_state(int state) const
    {
        return std::ranges::any_of(m_accepting_states, [&state](int acc) { return acc == state; });
    }

    [[nodiscard]] bool is_accepted(const tree::Tree& t) const
    {
        std::vector<tree::NodeId> nodes_to_process = {};

        auto states = std::vector<std::optional<int>>(t.nodes.size(), std::nullopt);

        for (const auto& n : t.nodes)
        {
            if (n.childs.empty())
            {
                nodes_to_process.emplace_back(n.id);
            }
        }

        while (!nodes_to_process.empty())
        {
            auto processed = nodes_to_process.back();
            nodes_to_process.pop_back();

            if (states[processed].has_value())
            {
                throw std::domain_error("Running automata on tree failed - node already has defined state");
            }

            std::vector<int> childs_states = {};
            for (auto child_index : t.nodes[processed].childs)
            {
                if (!states[child_index].has_value())
                {
                    throw std::domain_error(
                          "Running automata on tree failed - child of processed node has not defined state");
                }
                childs_states.emplace_back(states[child_index].value());
            }

            states[processed] = get_state(childs_states, t.nodes[processed].symbol);
            if (!states[processed].has_value())
            {
                return false;
            }

            if (t.nodes[processed].father.has_value())
            {
                bool can_be_add = true;
                for (const auto& child_id : t.nodes[t.nodes[processed].father.value()].childs)
                {
                    if (!states[child_id].has_value())
                    {
                        can_be_add = false;
                        break;
                    }
                }

                if (can_be_add)
                {
                    nodes_to_process.emplace_back(t.nodes[processed].father.value());
                }
            }
        }

        for (const auto& n : t.nodes)
        {
            if (!n.father.has_value())
            {
                if (!states[n.id].has_value())
                {
                    return false;
                }
                return is_accepted_state(states[n.id].value());
            }
        }

        return false;
    }


    [[nodiscard]] bool is_accepted(const tree::DAGTree& dag) const
    {
        std::vector<tree::NodeId> nodes_to_process = {};

        auto states = std::vector<std::optional<int>>(dag.nodes.size(), std::nullopt);

        for (const auto& n : dag.nodes)
        {
            if (n.childs.empty())
            {
                nodes_to_process.emplace_back(n.id);
            }
        }

        while (!nodes_to_process.empty())
        {
            auto processed = nodes_to_process.back();
            nodes_to_process.pop_back();

            if (states[processed].has_value())
            {
                throw std::domain_error("Running automata on tree failed - node already has defined state");
            }

            std::vector<int> childs_states = {};
            for (auto child_index : dag.nodes[processed].childs)
            {
                if (!states[child_index].has_value())
                {
                    throw std::domain_error(
                          "Running automata on tree failed - child of processed node has not defined state");
                }
                childs_states.emplace_back(states[child_index].value());
            }

            states[processed] = get_state(childs_states, dag.nodes[processed].symbol);
            if (!states[processed].has_value())
            {
                return false;
            }

            for (auto f : dag.nodes[processed].father)
            {
                bool can_be_add = true;
                for (const auto& child_id : dag.nodes[f].childs)
                {
                    if (!states[child_id].has_value())
                    {
                        can_be_add = false;
                        break;
                    }
                }

                if (can_be_add)
                {
                    nodes_to_process.emplace_back(f);
                }
            }
        }

        for (const auto& n : dag.nodes)
        {
            if (n.father.empty())
            {
                if (!states[n.id].has_value())
                {
                    return false;
                }
                return is_accepted_state(states[n.id].value());
            }
        }

        return false;
    }

    void set_final_state(int state)
    {
        m_accepting_states.emplace_back(state);
    }


    std::optional<tree::DAGTree> get_small_tree(int state)
    {
        if (small_trees[state].has_value())
        {
            return small_trees[state].value();
        }

        bool was_improved = true;
        while (was_improved)
        {
            was_improved = false;

            for (auto& tr : m_delta_function)
            {
                if (small_trees[tr.m_outgoing_state].has_value())
                {
                    continue;
                }

                bool children_ready = true;
                for (auto& child : tr.m_incoming_states)
                {
                    if (!small_trees[child].has_value())
                    {
                        children_ready = false;
                        break;
                    }
                }

                if (children_ready)
                {
                    tree::DAGTree res(m_alphabet);
                    res.add_node({}, tr.m_symbol);
                    for (auto& child : tr.m_incoming_states)
                    {
                        res.join_tree({ 0 }, get_small_tree(child).value());
                    }
                    res.dagify_bottom_up();
                    small_trees[tr.m_outgoing_state] = res;

                    was_improved = true;
                }
            }
        }

        return small_trees[state];
    }
};

} // namespace bottom_up_dtfta
