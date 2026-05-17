#pragma once
#include <functional>
#include <optional>
#include <unordered_set>

#include "BottomUpDTFA.h"

// States are functions from Z_n → Z_n represented as vectors
// f(x) = vec[x]
// Composition: (f ∘ g)(x) = f(g(x))
// φ is a fixed endomorphism on this monoid

struct FunctionState
{
    std::vector<int> mapping; // mapping[i] = f(i)

    bool operator==(const FunctionState& other) const
    {
        return mapping == other.mapping;
    }
};

struct FunctionStateHash
{
    size_t operator()(const FunctionState& f) const
    {
        size_t h = 0;
        for (int x : f.mapping)
        {
            h = h * 31 + x;
        }
        return h;
    }
};

// Compose two functions: (f ∘ g)(x) = f(g(x))
FunctionState compose(const FunctionState& f, const FunctionState& g)
{
    FunctionState result;
    result.mapping.resize(f.mapping.size());
    for (size_t i = 0; i < f.mapping.size(); i++)
    {
        result.mapping[i] = f.mapping[g.mapping[i]];
    }
    return result;
}

// Apply endomorphism φ to a function
// We define φ(f) = σ ∘ f ∘ σ^(-1) for some permutation σ
// This guarantees: φ(f ∘ g) = φ(f) ∘ φ(g)  (distributivity!)
FunctionState apply_phi(const FunctionState& f, const std::vector<int>& sigma, const std::vector<int>& sigma_inv)
{
    FunctionState result;
    int           n = f.mapping.size();
    result.mapping.resize(n);

    // φ(f)(x) = σ(f(σ^(-1)(x)))
    for (int x = 0; x < n; x++)
    {
        result.mapping[x] = sigma[f.mapping[sigma_inv[x]]];
    }

    return result;
}

std::optional<bottom_up_dtfta::BottomUpDTFA> quotient_distributivity_automata(
      int base_n,     // Size of domain Z_n (controls state explosion)
      int max_states, // Cap on number of states
      int zero_ary_symbols)
{
    srand(std::time(nullptr));

    // Generate a random permutation σ for defining φ
    std::vector<int> sigma(base_n), sigma_inv(base_n);
    for (int i = 0; i < base_n; i++)
    {
        sigma[i] = i;
    }
    for (int i = base_n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        std::swap(sigma[i], sigma[j]);
    }
    for (int i = 0; i < base_n; i++)
    {
        sigma_inv[sigma[i]] = i;
    }

    // Generate initial functions (leaf symbols)
    std::vector<FunctionState> leaf_functions(zero_ary_symbols);
    for (int i = 0; i < zero_ary_symbols; i++)
    {
        leaf_functions[i].mapping.resize(base_n);
        for (int j = 0; j < base_n; j++)
        {
            leaf_functions[i].mapping[j] = rand() % base_n;
        }
    }

    // BFS to discover reachable states under composition
    std::unordered_set<FunctionState, FunctionStateHash>      discovered;
    std::unordered_map<FunctionState, int, FunctionStateHash> state_id;
    std::vector<FunctionState>                                states;
    std::queue<FunctionState>                                 worklist;

    auto add_state = [&](const FunctionState& f) -> int
    {
        if (state_id.contains(f))
        {
            return state_id[f];
        }
        if (states.size() >= max_states)
        {
            return -1;
        }
        int id = states.size();
        states.push_back(f);
        state_id[f] = id;
        worklist.push(f);
        return id;
    };

    // Add leaf states
    for (const auto& lf : leaf_functions)
    {
        if (add_state(lf) == -1)
        {
            return std::nullopt;
        }
    }

    // Reserve space to prevent reallocation during iteration
    states.reserve(max_states);

    // Explore compositions AND phi-images together to ensure closure
    // We need to close under both composition and phi application
    while (!worklist.empty())
    {
        FunctionState current = worklist.front();
        worklist.pop();

        // Close under composition with all known states
        // Use index-based loop and copy to avoid iterator/reference invalidation
        size_t current_size = states.size();
        for (size_t i = 0; i < current_size; i++)
        {
            // Make a copy to avoid dangling reference if vector reallocates
            FunctionState other = states[i];

            FunctionState composed = compose(current, other);
            if (add_state(composed) == -1)
            {
                return std::nullopt;
            }

            // Also try reverse composition
            composed = compose(other, current);
            if (add_state(composed) == -1)
            {
                return std::nullopt;
            }
        }

        // Also close under phi application
        FunctionState phi_current = apply_phi(current, sigma, sigma_inv);
        if (add_state(phi_current) == -1)
        {
            return std::nullopt;
        }
    }

    int num_states = states.size();

    // Build the automaton
    alphabet::Alphabet alph;
    alph.add_symbol({ .m_arity = 2, .m_name = "f" });
    alph.add_symbol({ .m_arity = 1, .m_name = "g" });
    for (int i = 0; i < zero_ary_symbols; i++)
    {
        alph.add_symbol({ .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + i)) });
    }

    bottom_up_dtfta::BottomUpDTFA result(alph);

    // Leaf transitions
    for (int i = 0; i < zero_ary_symbols; i++)
    {
        result.add_transition({ .m_incoming_states = {},
                                .m_symbol          = std::string(1, static_cast<char>('a' + i)),
                                .m_outgoing_state  = state_id[leaf_functions[i]] });
    }

    // Binary operation f = composition
    for (int q1 = 0; q1 < num_states; q1++)
    {
        for (int q2 = 0; q2 < num_states; q2++)
        {
            FunctionState composed = compose(states[q1], states[q2]);
            result.add_transition({
                  .m_incoming_states = { q1, q2 },
                  .m_symbol          = "f",
                  .m_outgoing_state  = state_id[composed]
            });
        }
    }

    // Unary operation g = φ (the endomorphism)
    for (int q = 0; q < num_states; q++)
    {
        FunctionState phi_q = apply_phi(states[q], sigma, sigma_inv);
        // phi_q is guaranteed to be in state_id since we closed under phi during BFS
        result.add_transition({ .m_incoming_states = { q }, .m_symbol = "g", .m_outgoing_state = state_id[phi_q] });
    }

    // Set accepting states
    for (int i = 0; i < (num_states + 9) / 10; i++)
    {
        result.set_final_state(rand() % num_states);
    }

    result.set_states(num_states);
    return result;
}
