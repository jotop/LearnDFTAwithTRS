#pragma once
#include "DAGTree.h"


namespace tree {
class ContextDAGTree : public DAGTree
{
    std::optional<NodeId> hitch_id = std::nullopt;

    std::string hitch_string = "hitch";

public:
    ContextDAGTree(const alphabet::Alphabet& a)
          : DAGTree(a)
    {
        m_alphabet.add_symbol(alphabet::Symbol { .m_arity = 0, .m_name = hitch_string });
    }


    NodeId add_node(std::vector<NodeId> father, const std::string& symbol) override
    {
        return add_node(father, m_alphabet.get_symbol(symbol));
    }


    NodeId add_node(std::vector<NodeId> father, alphabet::Symbol symbol) override
    {
        if (symbol.m_name == hitch_string)
        {
            if (father.size() > 1 || hitch_id.has_value())
            {
                auto message = "Trying to add node to context with invalid data\n";
                std::cerr << message;
                throw std::invalid_argument(message);
            }
            hitch_id = DAGTree::add_node(father, symbol);
            return hitch_id.value();
        }

        for (auto& f : father)
        {
            if (nodes[f].symbol.m_name == hitch_string)
            {
                auto message = "Trying to add node to context hitch\n";
                std::cerr << message;
                throw std::invalid_argument(message);
            }
        }

        return DAGTree::add_node(father, symbol);
    }


    NodeId add_hitch(NodeId father)
    {
        if (father == -1)
        {
            hitch_id = DAGTree::add_node({}, m_alphabet.get_symbol(hitch_string));
            return hitch_id.value();
        }

        hitch_id = DAGTree::add_node({ father }, m_alphabet.get_symbol(hitch_string));
        return hitch_id.value();
    }


    static ContextDAGTree get_empty_context(const alphabet::Alphabet& a)
    {
        auto res = ContextDAGTree(a);
        res.add_node({}, res.get_alphabet().get_symbol(res.hitch_string));
        return res;
    }


    DAGTree apply_context(const DAGTree& t) const
    {
        if (!hitch_id.has_value())
        {
            auto message = "Context is invalid. There is no hitch in context\n";
            std::cerr << message;
            throw std::invalid_argument(message);
        }

        if (nodes[hitch_id.value()].father.empty())
        {
            return t;
        }

        return hitch_subtree_as_node(t, hitch_id.value());
    }


    static std::vector<ContextDAGTree> make_context(const DAGTree& tree, NodeId child_id)
    {
        if (tree.nodes[child_id].father.empty())
        {
            ContextDAGTree context(tree.get_alphabet());
            context.add_hitch(-1);

            return { context };
        }

        int child_appears = 0;
        for (auto& f : tree.nodes[child_id].father)
        {
            for (auto& c : tree.nodes[f].childs)
            {
                if (c == child_id)
                {
                    child_appears++;
                }
            }
        }

        if (child_appears>1)
        {
            std::vector<ContextDAGTree> res;
            for (auto f : tree.nodes[child_id].father)
            {
                ContextDAGTree context(tree.get_alphabet());
                context.nodes = tree.nodes;

                context.nodes.emplace_back(Node {
                      static_cast<int>(context.nodes.size()),
                      { f },
                      { .m_arity = 0, .m_name = "hitch" },
                      {}
                });

                context.hitch_id = context.nodes.size() - 1;

                int count_appears_in_spec_f = 0;
                for (int child : context.nodes[f].childs)
                {
                    if (child == child_id)
                    {
                        count_appears_in_spec_f++;
                    }
                }

                if (count_appears_in_spec_f == 1)
                {
                    for (int i = 0; i < context.nodes[child_id].father.size(); i++)
                    {
                        if (context.nodes[child_id].father[i] == f)
                        {
                            context.nodes[child_id].father.erase(std::next(context.nodes[child_id].father.begin(), i));
                            break;
                        }
                    }
                }

                for (int i = 0; i < context.nodes[f].childs.size(); i++)
                {
                    if (context.nodes[f].childs[i] == child_id)
                    {
                        ContextDAGTree temp(context);
                        temp.nodes[f].childs[i] = temp.nodes.size() - 1;
                        res.emplace_back(temp);
                    }
                }
                //res.emplace_back(context);
            }
            return res;
        }

        ContextDAGTree context(tree.get_alphabet());
        context.nodes = tree.nodes;

        auto father = context.nodes[child_id].father[0];

        context.nodes.emplace_back(Node {
              static_cast<int>(context.nodes.size()),
              { father },
              { .m_arity = 0, .m_name = "hitch" },
              {}
        });

        context.hitch_id = context.nodes.size() - 1;

        for (int i = 0; i < context.nodes[father].childs.size(); i++)
        {
            if (context.nodes[father].childs[i] == child_id)
            {
                context.nodes[father].childs[i] = context.nodes.size() - 1;
            }
        }

        for (int i = 0; i < context.nodes[child_id].father.size(); i++)
        {
            if (context.nodes[child_id].father[i] == father)
            {
                context.nodes[child_id].father.erase(std::next(context.nodes[child_id].father.begin(), i));
                break;
            }
        }

        std::unordered_map<NodeId, NodeId> old_to_new;
        std::unordered_set<NodeId> deleted_nodes;

        std::vector<Node> new_nodes;

        for (const auto& node : context.nodes)
        {
            std::vector<NodeId> new_fathers;
            for (auto father_id : node.father)
            {
                if (!deleted_nodes.contains(father_id))
                {
                    new_fathers.push_back(old_to_new[father_id]);
                }
            }

            if (node.id!=0 && new_fathers.empty())
            {
                deleted_nodes.insert(node.id);
                continue;
            }

            auto new_id = new_nodes.size();
            new_nodes.emplace_back(new_id, new_fathers, node.symbol, std::vector<int>{});
            old_to_new[node.id] = new_id;
        }

        for (const auto& node : context.nodes)
        {
            if (deleted_nodes.contains(node.id))
            {
                continue;
            }

            std::vector<NodeId> new_childs;
            for (auto child : node.childs)
            {
                new_childs.push_back(old_to_new[child]);
            }

            new_nodes[old_to_new[node.id]].childs = new_childs;
        }

        context.hitch_id = old_to_new[context.hitch_id.value()];
        context.nodes = new_nodes;
        return { context };
    }
};

} // namespace tree
