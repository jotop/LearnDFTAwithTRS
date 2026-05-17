#pragma once
#include <algorithm>
#include <functional>
#include <optional>
#include <utility>

#include "Alfabet.h"

#include <unordered_set>
#include <queue>


namespace tree {

typedef int NodeId;

class DAGTree
{
protected:
    alphabet::Alphabet m_alphabet;

    NodeId new_id() const
    {
        return static_cast<NodeId>(nodes.size());
    }

    // Genereted by AI
    static size_t node_hash(const alphabet::Symbol& symbol, const std::vector<size_t>& children_hashes)
    {
        size_t h = std::hash<std::string>()(symbol.m_name);

        h ^= std::hash<int>()(symbol.m_arity) + 0x9e3779b9 + (h << 6) + (h >> 2);

        for (auto c : children_hashes)
        {
            h ^= c + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }

    std::optional<size_t> m_tree_hash;

public:
    DAGTree(const alphabet::Alphabet& a)
          : m_alphabet(a)
    {}

    struct Node
    {
        NodeId              id;
        std::vector<NodeId> father;
        alphabet::Symbol    symbol;
        std::vector<NodeId> childs;
    };

    std::vector<Node>                   nodes;
    std::vector<std::optional<DAGTree>> subtrees;

    const alphabet::Alphabet& get_alphabet() const
    {
        return m_alphabet;
    }

    std::string to_string() const
    {
        std::string result;

        for (auto& n : nodes)
        {
            result += std::format("ID: {}, Symbol: {}, Childs: ", n.id, n.symbol.m_name);
            for (auto& c : n.childs)
            {
                result += std::format("{},", c);
            }

            result += "\n";
        }

        return result;
    }

    size_t tree_hash()
    {
        if (m_tree_hash.has_value())
        {
            return m_tree_hash.value();
        }

        std::queue<NodeId> q;
        for (NodeId i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].childs.empty())
            {
                q.push(i);
            }
        }

        std::vector<std::optional<size_t>> hashes(nodes.size(), std::nullopt);

        while (!q.empty())
        {
            auto current_node_id = q.front();
            q.pop();

            std::vector<size_t> ch_hashes;
            for (NodeId cid : nodes[current_node_id].childs)
            {
                ch_hashes.push_back(hashes[cid].value());
            }

            hashes[current_node_id] = node_hash(nodes[current_node_id].symbol, ch_hashes);

            for (auto father : nodes[current_node_id].father)
            {
                bool can_be_added = true;
                for (auto c : nodes[father].childs)
                {
                    if (!hashes[c].has_value())
                    {
                        can_be_added = false;
                        break;
                    }
                }

                if (can_be_added)
                {
                    q.push(father);
                }
            }
        }

        m_tree_hash = hashes[0];
        return m_tree_hash.value();
    }

    virtual NodeId add_node(std::vector<NodeId> father, const std::string& symbol)
    {
        return add_node(father, m_alphabet.get_symbol(symbol));
    }

    virtual NodeId add_node(std::vector<NodeId> father, alphabet::Symbol symbol)
    {
        subtrees.clear();

        m_tree_hash = std::nullopt;

        const auto id = new_id();
        nodes.emplace_back(Node { .id = id, .father = father, .symbol = std::move(symbol), .childs = {} });

        for (auto& f : father)
        {
            nodes[f].childs.emplace_back(id);

            if (nodes[f].childs.size() > nodes[f].symbol.m_arity)
            {
                std::cerr << std::format("The arity of node {} has been exceeded. Caused by node {}\n", f, id);

                throw std::invalid_argument(
                      std::format("The arity of node {} has been exceeded. Caused by node {}\n", f, id));
            }
        }

        return id;
    }


    void join_tree(std::vector<NodeId> father, const DAGTree& tree)
    {
        subtrees.clear();

        m_tree_hash = std::nullopt;

        size_t delta = nodes.size();
        for (auto n : tree.nodes)
        {
            std::vector<NodeId> fathers = {};
            if (n.father.empty())
            {
                fathers = father;
            }
            else
            {
                for (auto f : n.father)
                {
                    fathers.emplace_back(f + delta);
                }
            }

            std::vector<NodeId> childs = {};
            for (auto c : n.childs)
            {
                childs.emplace_back(c + delta);
            }

            nodes.emplace_back(n.id + delta, fathers, n.symbol, childs);

            if (!n.father.empty())
            {
                continue;
            }

            for (auto& f : father)
            {
                nodes[f].childs.emplace_back(delta);

                if (nodes[f].childs.size() > nodes[f].symbol.m_arity)
                {
                    std::cerr
                          << std::format("The arity of node {} has been exceeded. Caused in joining tree by node {}\n",
                                         f,
                                         delta);

                    throw std::invalid_argument(
                          std::format("The arity of node {} has been exceeded. Caused in joining tree by node {}\n",
                                      f,
                                      delta));
                }
            }
        }
    }


    [[nodiscard]] bool check_consistency() const
    {
        return std::ranges::all_of(nodes, [](const Node& n) { return n.symbol.m_arity == n.childs.size(); });
    }


    DAGTree extract_subtree(NodeId root_id)
    {
        if (subtrees.size() != nodes.size())
        {
            subtrees = std::vector<std::optional<DAGTree>>(nodes.size(), std::nullopt);
        }

        if (subtrees[root_id].has_value())
        {
            return subtrees[root_id].value();
        }

        DAGTree res = DAGTree(m_alphabet);
        res.add_node({}, nodes[root_id].symbol);

        for (auto& c : nodes[root_id].childs)
        {
            res.join_tree({ 0 }, extract_subtree(c));
        }
        res.dagify_bottom_up();
        subtrees[root_id] = res;

        return res;
    }


    // TODO: It can be more efficient
    std::vector<DAGTree> extract_all_unique_subtrees() const
    {
        std::vector<DAGTree>       result;
        std::unordered_set<size_t> seen_hashes;

        std::function<void(DAGTree&)> visit;

        visit = [&](DAGTree& tree)
        {
            size_t h = tree.tree_hash();

            if (seen_hashes.contains(h))
            {
                return;
            }

            seen_hashes.insert(h);
            result.push_back(tree);
            for (auto child_id : tree.nodes[0].childs)
            {
                DAGTree subtree = tree.extract_subtree(child_id);
                visit(subtree);
            }
        };

        visit(const_cast<DAGTree&>(*this));
        return result;
    }

    std::pair<DAGTree, std::vector<int>> delete_node(int node_to_delete) const
    {
        std::vector<Node> new_nodes;

        auto cast_index = [&](int i)
        {
            if (i == node_to_delete)
            {
                return int(nodes.size() - 1);
            }

            if (i < node_to_delete)
            {
                return i;
            }

            return i - 1;
        };

        auto cast_index_vec = [&](std::vector<int> i)
        {
            std::vector<int> res;
            for (auto j : i)
            {
                res.emplace_back(cast_index(j));
            }
            return res;
        };


        // move hitch to the end
        for (const auto& node : nodes)
        {
            if (node.id == node_to_delete)
            {
                continue;
            }


            new_nodes.emplace_back(cast_index(node.id),
                                   cast_index_vec(node.father),
                                   node.symbol,
                                   cast_index_vec(node.childs));
        }

        DAGTree res(m_alphabet);
        res.nodes = new_nodes;

        auto new_fathers = cast_index_vec(nodes[node_to_delete].father);

        return std::make_pair(res, new_fathers);
    }


    void add_subtree(const DAGTree& t, const std::vector<int>& new_fathers)
    {
        subtrees.clear();

        auto delta = nodes.size();

        for (const auto& node : t.nodes)
        {
            std::vector<int> childs;
            std::vector<int> fathers;

            if (node.father.empty())
            {
                fathers = new_fathers;
            }
            else
            {
                for (auto f : node.father)
                {
                    fathers.emplace_back(f + delta);
                }
            }

            for (auto c : node.childs)
            {
                childs.emplace_back(c + delta);
            }

            nodes.emplace_back(node.id + delta, fathers, node.symbol, childs);
        }
    }

    DAGTree hitch_subtree_as_node(const DAGTree& t, NodeId n) const
    {
        const auto deletion_result = delete_node(n);

        std::vector<Node> new_nodes = deletion_result.first.nodes;

        auto             tree_after_deletion = deletion_result.first;
        std::vector<int> new_fathers         = deletion_result.second;

        tree_after_deletion.add_subtree(t, new_fathers);
        return tree_after_deletion;
    }

    void dagify_bottom_up()
    {
        std::queue<NodeId> q;
        for (NodeId i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].childs.empty())
            {
                q.push(i);
            }
        }

        std::vector<std::optional<size_t>> hashes(nodes.size(), std::nullopt);

        std::unordered_map<size_t, NodeId> hash_to_node;
        std::unordered_map<NodeId, NodeId> old_to_new;
        std::vector<int>                   processed(nodes.size(), 0);

        while (!q.empty())
        {
            auto current_node_id = q.front();
            q.pop();

            std::vector<size_t> ch_hashes;
            for (NodeId cid : nodes[current_node_id].childs)
            {
                ch_hashes.push_back(hashes[cid].value());
            }

            size_t h = node_hash(nodes[current_node_id].symbol, ch_hashes);

            auto   it = hash_to_node.find(h);
            NodeId new_id;
            if (it != hash_to_node.end())
            {
                new_id = it->second;
            }
            else
            {
                new_id          = hash_to_node.size();
                hash_to_node[h] = new_id;
            }
            hashes[current_node_id]     = h;
            old_to_new[current_node_id] = new_id;

            for (auto father : nodes[current_node_id].father)
            {
                bool can_be_added = true;
                for (auto c : nodes[father].childs)
                {
                    if (!hashes[c].has_value())
                    {
                        can_be_added = false;
                        break;
                    }
                }

                if (can_be_added)
                {
                    q.push(father);
                }
            }
        }

        std::vector<Node> new_nodes(hash_to_node.size());
        std::vector<bool> visited(hash_to_node.size(), false);

        for (NodeId i = 0; i < nodes.size(); ++i)
        {
            NodeId new_id = old_to_new[i];
            if (visited[new_id])
            {
                for (NodeId fid : nodes[i].father)
                {
                    bool already_added_father = false;
                    for (auto father : new_nodes[new_id].father)
                    {
                        if (father == old_to_new[fid])
                        {
                            already_added_father = true;
                        }
                    }

                    if (!already_added_father)
                    {
                        new_nodes[new_id].father.push_back(old_to_new[fid]);
                    }
                }
                continue;
            }
            visited[new_id] = true;

            new_nodes[new_id].id = new_id;

            new_nodes[new_id].symbol = nodes[i].symbol;

            new_nodes[new_id].childs.clear();
            for (NodeId cid : nodes[i].childs)
            {
                new_nodes[new_id].childs.push_back(old_to_new[cid]);
            }

            new_nodes[new_id].father.clear();
            for (NodeId fid : nodes[i].father)
            {
                new_nodes[new_id].father.push_back(old_to_new[fid]);
            }
        }

        nodes.clear();

        std::queue<NodeId> to_proper_sequence;
        to_proper_sequence.push(old_to_new[0]);

        std::unordered_map<NodeId, NodeId> new_to_newer;


        while (!to_proper_sequence.empty())
        {
            auto curr = to_proper_sequence.front();
            to_proper_sequence.pop();

            if (new_to_newer[curr])
            {
                continue;
            }

            std::vector<NodeId> fathers;
            for (auto f : new_nodes[curr].father)
            {
                fathers.emplace_back(new_to_newer[f]);
            }

            new_to_newer[curr] = nodes.size();

            nodes.emplace_back(new_to_newer[curr], fathers, new_nodes[curr].symbol, new_nodes[curr].childs);
            //add_node(fathers, new_nodes[curr].symbol);

            for (auto c : new_nodes[curr].childs)
            {
                bool can_be_added = true;
                for (auto f : new_nodes[c].father)
                {
                    if (!new_to_newer.contains(f))
                    {
                        can_be_added = false;
                        break;
                    }
                }

                if (can_be_added)
                {
                    to_proper_sequence.push(c);
                }
            }
        }

        for (auto& n : nodes)
        {
            std::vector<int> childs;
            childs.reserve(n.childs.size());
            for (auto c : n.childs)
            {
                childs.emplace_back(new_to_newer[c]);
            }
            n.childs = childs;
        }
    }
};

} // namespace tree
