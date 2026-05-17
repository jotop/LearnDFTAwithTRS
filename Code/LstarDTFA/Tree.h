#pragma once
#include <algorithm>
#include <optional>
#include <utility>

#include "Alfabet.h"


namespace tree {

typedef int NodeId;

class Tree
{
    alphabet::Alphabet m_alphabet;

    NodeId new_id() const
    {
        return static_cast<NodeId>(nodes.size());
    }

public:
    Tree(const alphabet::Alphabet& a)
          : m_alphabet(a)
    {}

    struct Node
    {
        NodeId                id;
        std::optional<NodeId> father;
        alphabet::Symbol      symbol;
        std::vector<NodeId>   childs;
    };

    std::vector<Node> nodes;

    NodeId add_node(std::optional<NodeId> father, const std::string& symbol)
    {
        return add_node(father, m_alphabet.get_symbol(symbol));
    }

    NodeId add_node(std::optional<NodeId> father, alphabet::Symbol symbol)
    {
        const auto id = new_id();
        nodes.emplace_back(Node { .id = id, .father = father, .symbol = std::move(symbol), .childs = {} });

        if (!father.has_value())
        {
            return id;
        }

        nodes[father.value()].childs.emplace_back(id);

        if (nodes[father.value()].childs.size() > nodes[father.value()].symbol.m_arity)
        {
            std::cerr << std::format("The arity of node {} has been exceeded. Caused by node {}\n", father.value(), id);

            throw std::invalid_argument(
                  std::format("The arity of node {} has been exceeded. Caused by node {}\n", father.value(), id));
        }

        return id;
    }

    [[nodiscard]] bool check_consistency() const
    {
        return std::ranges::all_of(nodes, [](const Node& n) { return n.symbol.m_arity == n.childs.size(); });
    }
};

} // namespace tree
