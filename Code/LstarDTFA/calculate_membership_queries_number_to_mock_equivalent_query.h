#pragma once
#include "Alfabet.h"
#include "BottomUpDTFA.h"
#include "Counterexample.h"
#include "DAGTree.h"
#include "Lstar.h"
#include <random>
#include <map>


namespace mem_queries_to_eq {

std::vector<alphabet::Alphabet> get_potential_alphabets()
{
    std::vector<alphabet::Alphabet> vec;

    for (int i = 1; i <= 20; i++)
    {
        alphabet::Alphabet a;
        a.add_symbol(alphabet::Symbol { .m_arity = 2, .m_name = "f" });

        for (int j = 0; j < i; j++)
        {
            a.add_symbol(alphabet::Symbol {.m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + j)) + "0" });
        }
        vec.emplace_back(a);
    }

    for (int i = 1; i <= 20; i++)
    {
        for (int k = 1; k <= 3; k++)
        {
            alphabet::Alphabet a;
            a.add_symbol(alphabet::Symbol { .m_arity = 2, .m_name = "f" });
            for (int j = 0; j < k; j++)
            {
                a.add_symbol(
                      alphabet::Symbol { .m_arity = 1, .m_name = std::string(1, static_cast<char>('a' + j)) + "1" });
            }

            for (int j = 0; j < i; j++)
            {
                a.add_symbol(
                      alphabet::Symbol { .m_arity = 0, .m_name = std::string(1, static_cast<char>('a' + j)) + "0" });
            }
            vec.emplace_back(a);
        }
    }

    return vec;
}

alphabet::Alphabet get_random_alphabet()
{
    const auto vec = get_potential_alphabets();

    std::random_device                    rd;
    std::mt19937                          gen(rd());
    std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);

    return vec[dist(gen)];
}

bottom_up_dtfta::BottomUpDTFA get_random_dtfa()
{
    std::random_device rd;
    std::mt19937       gen(rd());

    auto alphabet = get_random_alphabet();

    // Random number of states
    std::uniform_int_distribution<int> states_dist(10, 200);
    int                                num_states = states_dist(gen);

    // Randomly select accepting states (each state has 20% chance of being accepting)
    std::vector<int>                   accepting_states;
    std::uniform_int_distribution<int> accept_dist(0, 10);
    for (int i = 0; i < num_states; i++)
    {
        if (accept_dist(gen) >= 8)
        {
            accepting_states.emplace_back(i);
        }
    }

    // Ensure at least one accepting state
    if (accepting_states.empty())
    {
        std::uniform_int_distribution<int> pick_dist(0, num_states - 1);
        accepting_states.emplace_back(pick_dist(gen));
    }

    bottom_up_dtfta::BottomUpDTFA dtfa(alphabet, num_states, accepting_states);

    std::uniform_int_distribution<int> state_dist(0, num_states - 1);

    // For each symbol, generate transitions for all combinations of incoming states
    for (const auto& symbol : alphabet.get_symbols())
    {
        if (symbol.m_arity == 0)
        {
            // Nullary symbol: one transition with empty incoming states
            dtfa.add_transition(
                  { .m_incoming_states = {}, .m_symbol = symbol.m_name, .m_outgoing_state = state_dist(gen) });
            continue;
        }

        // Generate all combinations of incoming states for this arity
        // Total combinations = num_states ^ arity
        size_t total_combinations = 1;
        for (size_t a = 0; a < symbol.m_arity; a++)
        {
            total_combinations *= num_states;
        }

        for (size_t combo = 0; combo < total_combinations; combo++)
        {
            std::vector<int> incoming_states(symbol.m_arity);
            size_t           temp = combo;
            for (size_t a = 0; a < symbol.m_arity; a++)
            {
                incoming_states[a]  = static_cast<int>(temp % num_states);
                temp               /= num_states;
            }

            dtfa.add_transition({ .m_incoming_states = incoming_states,
                                  .m_symbol          = symbol.m_name,
                                  .m_outgoing_state  = state_dist(gen) });
        }
    }

    return dtfa;
}

bool are_all_states_reachable(const bottom_up_dtfta::BottomUpDTFA& dtfa)
{
    const int         num_states = dtfa.get_states();
    std::vector<bool> reachable(num_states, false);

    // Start with states reachable from nullary symbols (arity 0)
    for (const auto& symbol : dtfa.get_alphabet().get_symbols())
    {
        if (symbol.m_arity == 0)
        {
            auto state = dtfa.get_state({}, symbol);
            if (state.has_value())
            {
                reachable[state.value()] = true;
            }
        }
    }

    // Fixed-point iteration: discover new reachable states
    bool changed = true;
    while (changed)
    {
        changed = false;

        for (const auto& transition : dtfa.get_delta_function())
        {
            // Skip if outgoing state is already reachable
            if (reachable[transition.m_outgoing_state])
            {
                continue;
            }

            // Check if all incoming states are reachable
            bool all_incoming_reachable = true;
            for (int incoming : transition.m_incoming_states)
            {
                if (!reachable[incoming])
                {
                    all_incoming_reachable = false;
                    break;
                }
            }

            if (all_incoming_reachable)
            {
                reachable[transition.m_outgoing_state] = true;
                changed                                = true;
            }
        }
    }

    // Check if all states are reachable
    for (int i = 0; i < num_states; i++)
    {
        if (!reachable[i])
        {
            return false;
        }
    }

    return true;
}

bottom_up_dtfta::BottomUpDTFA get_random_reachable_dtfa()
{
    while (true)
    {
        auto dtfa = get_random_dtfa();
        if (are_all_states_reachable(dtfa))
        {
            return dtfa;
        }
    }
}


// Generate a random tree of a given max depth over the alphabet
tree::DAGTree generate_random_tree(const alphabet::Alphabet& alphabet, int max_depth, std::mt19937& gen)
{
    tree::DAGTree tree(alphabet);

    const auto& symbols = alphabet.get_symbols();

    // Separate symbols by arity
    std::vector<const alphabet::Symbol*> nullary_symbols;
    for (const auto& sym : symbols)
    {
        if (sym.m_arity == 0)
        {
            nullary_symbols.push_back(&sym);
        }
    }

    // Recursive tree builder
    std::function<void(std::vector<tree::NodeId>, int)> build = [&](std::vector<tree::NodeId> father, int depth)
    {
        if (depth <= 0)
        {
            // Must place a nullary symbol (leaf)
            std::uniform_int_distribution<size_t> leaf_dist(0, nullary_symbols.size() - 1);
            tree.add_node(father, *nullary_symbols[leaf_dist(gen)]);
            return;
        }

        // Choose: leaf or internal node
        std::uniform_int_distribution<int> choice(0, static_cast<int>(symbols.size()) - 1);
        int                                idx = choice(gen);

        // Pick a random symbol
        const auto& sym = symbols[idx];

        auto node_id = tree.add_node(father, sym);
        for (size_t i = 0; i < sym.m_arity; i++)
        {
            build({ node_id }, depth - 1);
        }
    };

    build({}, max_depth);
    return tree;
}


// BlindTeacher: simulates equivalence query using random membership queries.
// Returns the number of random trees tested before finding a counterexample,
// or -1 if no counterexample was found within the budget.
int blind_equivalence_query(const bottom_up_dtfta::BottomUpDTFA& target,
                            const bottom_up_dtfta::BottomUpDTFA& candidate,
                            int                                  num_random_trees,
                            int                                  max_tree_depth,
                            std::mt19937&                        gen)
{
    const auto& alphabet = target.get_alphabet();

    for (int i = 0; i < num_random_trees; i++)
    {
        auto tree = generate_random_tree(alphabet, max_tree_depth, gen);

        bool target_accepts    = target.is_accepted(tree);
        bool candidate_accepts = candidate.is_accepted(tree);

        if (target_accepts != candidate_accepts)
        {
            return i + 1; // Found counterexample after i+1 queries
        }
    }

    return -1; // No counterexample found
}


// Result structure for the experiment
struct ConfidenceResult
{
    int num_states;
    int queries_for_70_percent;
    int queries_for_80_percent;
    int queries_for_90_percent;
    int queries_for_99_percent;
};


// Run the experiment:
// For a given number of states, generate many pairs of non-equivalent automata,
// measure how many random membership queries are needed to find a counterexample,
// and determine the thresholds for 70%, 80%, 90%, 99% confidence.
ConfidenceResult run_experiment_for_states(int num_states, int num_trials, int max_queries_budget, int max_tree_depth)
{
    std::random_device rd;
    std::mt19937       gen(rd());

    // Collect the number of queries needed to find counterexample for each trial
    std::vector<int> queries_needed;

    for (int trial = 0; trial < num_trials; trial++)
    {
        // Generate target automaton with the specified number of states
        auto alphabet = get_random_alphabet();

        std::uniform_int_distribution<int> accept_dist(0, 10);
        std::vector<int>                   accepting_states;
        for (int i = 0; i < num_states; i++)
        {
            if (accept_dist(gen) >= 8)
            {
                accepting_states.emplace_back(i);
            }
        }
        if (accepting_states.empty())
        {
            std::uniform_int_distribution<int> pick_dist(0, num_states - 1);
            accepting_states.emplace_back(pick_dist(gen));
        }

        bottom_up_dtfta::BottomUpDTFA      target(alphabet, num_states, accepting_states);
        std::uniform_int_distribution<int> state_dist(0, num_states - 1);

        for (const auto& symbol : alphabet.get_symbols())
        {
            if (symbol.m_arity == 0)
            {
                target.add_transition(
                      { .m_incoming_states = {}, .m_symbol = symbol.m_name, .m_outgoing_state = state_dist(gen) });
                continue;
            }
            size_t total_combinations = 1;
            for (size_t a = 0; a < symbol.m_arity; a++)
            {
                total_combinations *= num_states;
            }

            for (size_t combo = 0; combo < total_combinations; combo++)
            {
                std::vector<int> incoming(symbol.m_arity);
                size_t           temp = combo;
                for (size_t a = 0; a < symbol.m_arity; a++)
                {
                    incoming[a]  = static_cast<int>(temp % num_states);
                    temp        /= num_states;
                }
                target.add_transition({ .m_incoming_states = incoming,
                                        .m_symbol          = symbol.m_name,
                                        .m_outgoing_state  = state_dist(gen) });
            }
        }

        // Generate candidate by copying target and perturbing a few transitions.
        // This simulates the real learning scenario where the hypothesis is close
        // to the target but differs in a small number of transitions.
        bottom_up_dtfta::BottomUpDTFA candidate(alphabet, num_states, accepting_states);

        const auto& target_transitions = target.get_delta_function();

        // Pick 1 to 3 transitions to perturb
        std::uniform_int_distribution<size_t> num_perturb_dist(1, std::min(size_t(3), target_transitions.size()));
        size_t num_perturbations = num_perturb_dist(gen);

        std::uniform_int_distribution<size_t> trans_idx_dist(0, target_transitions.size() - 1);
        std::unordered_set<size_t> perturbed_indices;
        while (perturbed_indices.size() < num_perturbations)
        {
            perturbed_indices.insert(trans_idx_dist(gen));
        }

        for (size_t i = 0; i < target_transitions.size(); i++)
        {
            auto t = target_transitions[i];
            if (perturbed_indices.contains(i))
            {
                // Change the outgoing state to a different one
                int new_state = state_dist(gen);
                while (new_state == t.m_outgoing_state && num_states > 1)
                {
                    new_state = state_dist(gen);
                }
                t.m_outgoing_state = new_state;
            }
            candidate.add_transition(t);
        }

        // Verify they are actually different using the real equivalence check
        auto real_counterexample = counterexample::find_counterexample(target, candidate);
        if (!real_counterexample.has_value())
        {
            // They happen to be equivalent, skip this trial
            trial--;
            continue;
        }

        // Now measure how many random queries it takes to find a counterexample
        int result = blind_equivalence_query(target, candidate, max_queries_budget, max_tree_depth, gen);

        if (result == -1)
        {
            queries_needed.push_back(max_queries_budget);
        }
        else
        {
            queries_needed.push_back(result);
        }
    }

    // Sort to find percentiles
    std::sort(queries_needed.begin(), queries_needed.end());

    auto get_percentile = [&](double percentile) -> int
    {
        size_t idx = static_cast<size_t>(std::ceil(percentile * queries_needed.size())) - 1;
        idx        = std::min(idx, queries_needed.size() - 1);
        return queries_needed[idx];
    };

    return ConfidenceResult {
        .num_states             = num_states,
        .queries_for_70_percent = get_percentile(0.70),
        .queries_for_80_percent = get_percentile(0.80),
        .queries_for_90_percent = get_percentile(0.90),
        .queries_for_99_percent = get_percentile(0.99),
    };
}


// Run the full experiment across multiple state counts
std::vector<ConfidenceResult> run_full_experiment(const std::vector<int>& state_counts,
                                                  int                     num_trials         = 200,
                                                  int                     max_queries_budget = 10000,
                                                  int                     max_tree_depth     = 30)
{
    std::vector<ConfidenceResult> results;

    for (int num_states : state_counts)
    {
        std::cout << std::format("Running experiment for {} states...\n", num_states);
        auto result = run_experiment_for_states(num_states, num_trials, max_queries_budget, max_tree_depth);
        results.push_back(result);

        std::cout << std::format("  States: {} | 70%%: {} | 80%%: {} | 90%%: {} | 99%%: {} queries\n",
                                 result.num_states,
                                 result.queries_for_70_percent,
                                 result.queries_for_80_percent,
                                 result.queries_for_90_percent,
                                 result.queries_for_99_percent);
    }

    return results;
}


} // namespace mem_queries_to_eq
