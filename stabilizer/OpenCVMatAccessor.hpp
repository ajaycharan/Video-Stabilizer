#ifndef OPENCVMATACCESSOR_HPP_
#define OPENCVMATACCESSOR_HPP_

#include "opencv2/core.hpp"
#include "boost/optional.hpp"
#include <array>

class OpenCVMatAccessor
{
public:
    using NodeIndex = cv::Point2i;
    using Value = uchar;
    using Data = cv::Mat;

    static const Value inf = 255;

    OpenCVMatAccessor(Data data) {
        data_ = data;
        mask_ = cv::Mat::zeros(data.rows, data.cols, CV_8U);
    }

    NodeIndex get_source() {
        auto new_node = NodeIndex{0, 0};
        mask_.at<uchar>(new_node) = 1;
        return NodeIndex{0, 0};
    }

    Value value (NodeIndex node) {
        uchar result  = data_.at<uchar>(node);
        return result;
    }


    boost::optional<NodeIndex> get_next_neighbor (NodeIndex node);

private:
    Data data_;
    cv::Mat mask_;
};

class OpenCVMatMserAnalyzer {
public:
    struct ComponentStats {
        unsigned int age = 0;
        unsigned int N = 0;
        cv::Vec2f mean = cv::Vec2f(0., 0.);
        cv::Matx22f cov = cv::Matx22f(0., 0., 0., 0.);
        float stability = 0.;
    };

    struct Component {
        uchar level;
        ComponentStats stats;
        std::vector<ComponentStats> history;

        Component (uchar value) : level(value) {}
    };

    using Result = std::vector<ComponentStats>;

    uchar get_level (Component& comp) {
        return comp.level;
    }

    void add_node( typename cv::Point2i node, uchar level, Component& component);

    void merge_component_into (Component& comp1, Component& comp2, uchar level);


    // TODO: still makes a deep copy -> has to be optimized (would calling std::move be save?
    Result get_result() { return result_; }

    bool is_finished() {
        return finished_;
    }

private:
    Result result_;
    bool finished_ = false;
    const unsigned int min_N_ = 200;
    const unsigned int max_N_ = 14400;
    const uchar delta_ = 5;
    const float min_stability_ = 20.;

    void merge_componentstats_into_(const ComponentStats& comp1, ComponentStats& comp2);
    void calculate_stability(Component& comp);
    void check_mser_ (Component& comp);
};


class OpenCVMatPriorityQueue {
public:
    void push(cv::Point2i point, uchar value) {
        points_[value].push_back(point);
        minimum_ = std::min(minimum_, value);
    }

    boost::optional<cv::Point2i> pop();

private:
    std::array<std::vector<cv::Point2i>, 256> points_;
    uchar minimum_ = 255;
};



inline boost::optional<OpenCVMatAccessor::NodeIndex> OpenCVMatAccessor::get_next_neighbor(OpenCVMatAccessor::NodeIndex node) {
    uchar& mask_value = mask_.at<uchar>(node);

    while (mask_value < 5) {
        NodeIndex next_node;

        switch (mask_value) {
        case 0:
            next_node = node;
        case 1:
            next_node = NodeIndex{node.x + 1, node.y};
            break;
        case 2:
            next_node = NodeIndex{node.x, node.y + 1};
            break;
        case 3:
            next_node = NodeIndex{node.x - 1, node.y};
            break;
        case 4:
            next_node = NodeIndex{node.x, node.y - 1};
            break;
        }
        ++mask_value;

        if (   0 <= next_node.x  && next_node.x < data_.cols
               && 0 <= next_node.y  && next_node.y < data_.rows ) {
            uchar& next_mask_value = mask_.at<uchar>(next_node);
            if (next_mask_value == 0) {
                ++next_mask_value;
                return next_node;
            }
        }
    }
    return boost::none;
}

void OpenCVMatMserAnalyzer::add_node(cv::Point2i node, uchar level, OpenCVMatMserAnalyzer::Component &component) {
    ComponentStats node_comp;
    node_comp.mean = cv::Vec2f(node.x, node.y);
    node_comp.N = 1;

    if (level > component.level) {
        for (int i=0; i<level - component.level; ++i)
            component.history.push_back(component.stats);
        component.level = level;
        check_mser_(component);
    }
    merge_componentstats_into_(node_comp, component.stats);
    calculate_stability(component);

}

void OpenCVMatMserAnalyzer::merge_component_into(OpenCVMatMserAnalyzer::Component &comp1, OpenCVMatMserAnalyzer::Component &comp2, uchar level) {
    // take the history of the winner
    Component* winner;
    if (comp1.stats.N > comp2.stats.N) {
        winner = &comp1;
        comp2.history = std::move(comp1.history);
    } else {
        winner = &comp2;
    }

    // update history
    if (level > winner->level) {
        for (int i=0; i<level - winner->level; ++i)
            comp2.history.push_back(winner->stats);
        comp2.level = level;
        check_mser_(comp2);
    }

    merge_componentstats_into_(comp1.stats, comp2.stats);
    calculate_stability(comp2);
}

inline void OpenCVMatMserAnalyzer::merge_componentstats_into_(const OpenCVMatMserAnalyzer::ComponentStats &comp1, OpenCVMatMserAnalyzer::ComponentStats &comp2) {
    auto p = float(comp1.N) / float(comp1.N + comp2.N);
    auto q = float(comp2.N) / float(comp1.N + comp2.N);

    for (auto i : {0, 1})
        for (auto j : {0, 1})
            comp2.cov(i, j) = p * comp1.cov(i, j) + q * comp2.cov(i, j)
                    + p*q*(comp2.mean(i) - comp1.mean(i)) * (comp2.mean(j) - comp1.mean(j));

    comp2.N = comp1.N + comp2.N;
    comp2.mean = p * comp1.mean + q * comp2.mean;
}

void OpenCVMatMserAnalyzer::calculate_stability(OpenCVMatMserAnalyzer::Component &comp) {
    if (comp.history.size() < delta_) {
        comp.stats.stability = 0;
    } else {
        auto old_N = comp.history.rbegin()[delta_-1].N;
        comp.stats.stability = static_cast<float>(delta_ * old_N)/(comp.stats.N - old_N);
    }

}

void OpenCVMatMserAnalyzer::check_mser_(OpenCVMatMserAnalyzer::Component &comp) {
    if (comp.history.size() >= 3) {
        auto& succ = comp.history.rbegin()[0];
        auto& examinee = comp.history.rbegin()[1];
        auto& pred = comp.history.rbegin()[2];

        if (examinee.stability > pred.stability && examinee.stability > succ.stability
             && min_N_ <= examinee.N && examinee.N <= max_N_
             && examinee.stability >= min_stability_)
                result_.push_back(examinee);
    }
}



inline boost::optional<cv::Point2i> OpenCVMatPriorityQueue::pop() {
    if (points_[minimum_].empty())
        return boost::none;
    else {
        auto next_point = points_[minimum_].back();
        points_[minimum_].pop_back();
        while (minimum_ < 255 && points_[minimum_].empty())
            ++minimum_;
        return next_point;
    }
}

#endif // OPENCVMATACCESSOR_HPP_





