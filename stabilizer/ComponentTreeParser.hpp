#ifndef COMPONENTTREEPARSER_HPP
#define COMPONENTTREEPARSER_HPP

#include <vector>
#include <stack>
#include <queue>
#include <iostream>
#include <boost/optional.hpp>

// declarations

/*
G must provide:
  NodeIndex
  Node
  Value // must be totally ordered by <
  Data

  static const Value inf

  G(const Data&)
  NodeIndex get_source()

  Value value (NodeIndex node);
  Node node (NodeIndex node);
  boost::optional<NodeIndex> get_next_neighbor (NodeIndex node);

A must provide:
  ComponentIndex
  Result

  void add_node(G::Node, G::Value)
  ComponentIndex add_component(G::Value)

  ComponentIndex merge_components(ComponentIndex comp1, ComponentIndex comp2) \\ 1st into 2nd

P must provide:
  void push(NodeIndex)
  boost::optional<NodeIndex> pop()

  TODO: put current levels in analyzer instead of parser
  TODO: use size hints from accessor to preallocate component stack
  TODO: better way to return analyzers results?
*/

template <typename G, typename A>
// requires GraphAccessor<G>
//      &&  ComponentAnalyzer<A>
class ComponentTreeParser {
    // interface
    public:
    using GraphAccessor = G;
    using NodeIndex = typename G::NodeIndex;
    using Node = typename G::Node;
    using Data = typename G::Data;
    using Value = typename G::Value;
    using Analyzer = A;
    using Result = typename A::Result;
    using Component = typename A::Component;

    using PriorityQueue = typename G::PriorityQueue;

    ComponentTreeParser (bool inverted = false)
        : inverted_(inverted) {}

    Result operator() (const Data& data) {
        auto graph = GraphAccessor(data);
        auto analyzer = Analyzer{};
        auto boundary_nodes = PriorityQueue{inverted_};
        return parse_(graph, analyzer, boundary_nodes);
    }

    Result operator() (GraphAccessor& graph, Analyzer& analyzer) {
        auto boundary_nodes = PriorityQueue{inverted_};
        return parse_(graph, analyzer, boundary_nodes);
    }

    Result operator() (GraphAccessor& graph, Analyzer& analyzer, PriorityQueue& boundary_nodes) {
        return parse_(graph, analyzer, boundary_nodes);
    }

    bool inverted() const {
        return inverted_;
    }

    void set_inverted (bool inv) {
        inverted_ = inv;
    }

    // implementation
    private:

    struct ComponentStack {
        public:
        ComponentStack (const ComponentTreeParser& parser, Analyzer& analyzer)
            : parser_(parser), analyzer_(analyzer), components_() {
            components_.push_back(analyzer_.add_component(parser_.inf()));
        }

        void push_component(Node node, Value level) {
            components_.push_back(analyzer_.add_component(node, level));
            //current_levels_.push_back(level);
        }
        void push_node(Node node, Value level) {
            analyzer_.add_node(node, level, components_.back());
        }

        void raise_level(Value level);

       // std::vector<Value> current_levels_;
        Analyzer& analyzer_;
        const ComponentTreeParser& parser_;
        std::vector<Component> components_;
    };


    // actual algorithm
    Result parse_(GraphAccessor& graph, Analyzer& analyzer, PriorityQueue& boundary_nodes);

    bool less (Value val1, Value val2) const {
        if (!inverted_)
            return GraphAccessor::less(val1, val2);
        else
            return GraphAccessor::less(val2, val1);
    }

    Value inf () const {
        if (!inverted_)
            return GraphAccessor::inf;
        else
            return GraphAccessor::minf;
    }

    bool inverted_ = false;

};

// definitions

template <typename G, typename A>
typename ComponentTreeParser<G,A>::Result ComponentTreeParser<G,A>::parse_(
    typename ComponentTreeParser<G,A>::GraphAccessor& graph,
    typename ComponentTreeParser<G,A>::Analyzer& analyzer,
    typename ComponentTreeParser<G,A>::PriorityQueue& boundary_nodes) {
    // data structures

    auto component_stack = ComponentStack{*this, analyzer};
    //auto boundary_nodes = std::priority_queue<NodeIndex, std::vector<NodeIndex>, NodePriorityLess>{NodePriorityLess{graph}};

    // initialize
    auto source_node = graph.get_source();
    boundary_nodes.push(source_node, graph.value(source_node));
    bool flowingdown_phase = true;
    //auto current_node = graph.get_source();

    // we are done, when there is no boundary node left
    while (auto current_node_or_none = boundary_nodes.pop()) {
        // get next node
        auto current_node = *current_node_or_none;
        component_stack.raise_level(graph.value(current_node));

        /*
            std::cout << "Current node: " << current_node
                      << " Value = " << static_cast<int>(graph.value(current_node)) << std::endl;
            */

        // explore neighborhood of current node
        // the accessor has to make sure, that we access every node only once
        while (auto neighbor_or_none = graph.get_next_neighbor(current_node)) {
            auto neighbor_node = *neighbor_or_none;
            // flow (further) down?
            if (less(graph.value(neighbor_node), graph.value(current_node))) {
                flowingdown_phase = true;
                boundary_nodes.push(current_node, graph.value(current_node));
                current_node = neighbor_node;
            } else {
                boundary_nodes.push(neighbor_node, graph.value(neighbor_node));
            }
        }

        // new minimum found?
        if (flowingdown_phase) {
            component_stack.push_component(graph.node(current_node), graph.value(current_node));
            flowingdown_phase = false;
        } else {
            component_stack.push_node(graph.node(current_node), graph.value(current_node));
        }
    }
    return analyzer.get_result();
}

template <typename G, typename A>
void ComponentTreeParser<G,A>::ComponentStack::raise_level(ComponentTreeParser<G,A>::Value level) {
    while (parser_.less(analyzer_.get_level(components_.back()), level)) {
        // level of second last component (exists, since current_level < inf)
        auto next_level = analyzer_.get_level(components_.rbegin()[1]);
        if  (parser_.less(level, next_level)) {
            analyzer_.raise_level(components_.back(), level);
        } else {
            // is it correct to use level instead of next_level here?
            analyzer_.merge_component_into(components_.rbegin()[0], components_.rbegin()[1], level);
            components_.pop_back();
        }
    }
}

#endif // COMPONENTTREEPARSER_HPP



