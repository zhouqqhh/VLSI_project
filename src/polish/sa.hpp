// test.cpp: testcases for slicing tree.
// Author: LYL

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>

#include "polish_tree.hpp"

using namespace std;
using namespace polish;

#define MY_ASSERT(expr) !!(expr) || error(#expr, __LINE__)
#define SHOW_STARTING_TEST() do { \
cout << "Starting " << __FUNCTION__ << "..." << endl; } while (false)

namespace {

    bool error(const char *expr, int line) {
        cerr << "Debug assertion failed: " << expr
            << ", test.cpp (" << line << ")" << endl;
        exit(EXIT_FAILURE);
        return false;
    }

    enum class operation_type {
        M1, M2, M3
    };

    struct operation {
        operation_type type;
        int target1;
        int target2;
    };

    class SA {
    public:
        using combine_type = typename basic_polish_node::combine_type;
        using tree_type = polish::polish_tree<>;
        using vctr_tree_type = polish::vectorized_polish_tree<>;
        using const_iterator = typename vctr_tree_type::const_iterator;

        SA(vctr_tree_type* vtree_in, float init_accept_rate, float cooldown_speed_in,
            int balance_minstep_in, float ending_temperature_in)
        : /*eng_(std::random_device{}()), */line_(80, '-') {
            vtree_ = *vtree_in;
            srand((unsigned)time(NULL));
            init_vbuf();
            print();
            print_tree(vtree_);
            temperature = count_init_temprature(init_accept_rate);
            std::cout << "init temperature " << temperature << endl;
            cooldown_speed = cooldown_speed_in;
            accept_under_currentT = total_under_currentT = 0;
            balance_minstep = balance_minstep_in;
            ending_temperature = ending_temperature_in;
            best_solution = -1;
        }

        void take_step() {
            // std::cout << "step" << endl;
            float pre_min_area, post_min_area;
            pre_min_area = count_min_area();
            struct operation op = random_operation();
            struct operation op_final = check_valid_and_go(op);
            post_min_area = count_min_area();
            if (pre_min_area <= post_min_area) { //probably accept
                float acc_rate = exp((pre_min_area - post_min_area) / temperature);
                if (((float)rand() / (float)RAND_MAX) <= acc_rate) {
                    accept_under_currentT++;
                    total_under_currentT++;
                } else {
                    goto_neighbor(op_final);   //recover previous state
                    total_under_currentT++;
                }
            } else { //accept
                accept_under_currentT++;
                total_under_currentT++;
            }
        }

        //only if accept rate > constant1 and total step > constant2
        bool reach_balance() {
            return accept_under_currentT > balance_minstep;
        }

        void cool_down_by_ratio() {
            std::cout << "current area : " << count_min_area() << endl;
            temperature = temperature * (1 - cooldown_speed);
            accept_under_currentT = total_under_currentT = 0;
        }

        void cool_down_by_speed() {
            std::cout << "current area : " <<  count_min_area() << endl;
            temperature = temperature - cooldown_speed;
            accept_under_currentT = total_under_currentT = 0;
        }

        bool reach_end() {
            return temperature <= ending_temperature;
        }

        void print() {
            print_tree(vtree_);
        }

        float current_solution() {
            return best_solution;
        }

    private:
        template<typename Tree>
        static std::ostream &print_tree(const Tree &t,
            std::ostream &os = std::cout) {
            for (const auto &e : t)
                os << e << " ";
            os << endl;
            return t.print_tree(os, 2);
        }

        void init_vbuf() {
            const_iterator it = vtree_.begin();
            while (it != vtree_.end()) {
                vbuf_.push_back(it);
                it++;
            }
            std::cout << vbuf_.size() << endl;
            // vbuf_.assign(vtree_.begin(), vtree_.end());
        }

        void load_tree() {
            yal::Module m;
            m.xpos.push_back(0);
            m.ypos.push_back(0);
            m.xpos.push_back(30);
            m.ypos.push_back(20);
            modules_.assign(12, m);
            expr_ = {
                0, 1, expression::COMBINE_VERTICAL,
                2, 3, expression::COMBINE_VERTICAL, 4, 5,
                expression::COMBINE_VERTICAL, expression::COMBINE_VERTICAL,
                expression::COMBINE_VERTICAL,
                6, 7, expression::COMBINE_VERTICAL,
                8, 9, expression::COMBINE_VERTICAL, 10, 11,
                expression::COMBINE_VERTICAL, expression::COMBINE_VERTICAL,
                expression::COMBINE_VERTICAL, expression::COMBINE_VERTICAL
            };
            vtree_.construct(modules_, expr_);
        }

        float count_init_temprature(float init_accept_rate) {
            float init_min_area = count_min_area();
            float post_min_area, total_drop;
            int N = 100;
            for (int i = 0; i < N; i++) {
                struct operation op = random_operation();
                struct operation op_final = check_valid_and_go(op);
                print();
                post_min_area = count_min_area();
                total_drop += abs(init_min_area - post_min_area);
                goto_neighbor(op_final);   //recover previous state
            }
            return - total_drop / (100 * log(init_accept_rate));
        }

        float count_min_area() {
            // print_coord_list((std::prev(vbuf_in.end()))->points);
            float min_area = -1;
            for (auto &&e : vbuf_.back()->points) {
                if (e.first * e.second < min_area || min_area < 0) {
                    min_area = e.first * e.second;
                }
            }
            if (min_area < best_solution || best_solution < 0) {
                best_solution = min_area;
                best_tree = vtree_;
            }
            return min_area;
        }

        void goto_neighbor(struct operation op) {
            if (op.type == operation_type::M2) {
                vtree_.invert_chain(vbuf_[op.target1]);
            } else {
                vtree_.swap_nodes(vbuf_[op.target1], vbuf_[op.target2]);
                std::swap(vbuf_[op.target1], vbuf_[op.target2]);
            }
        }

        //检查操作合法性，校正并执行操作
        struct operation check_valid_and_go(struct operation op) {
            if (op.type == operation_type::M1) {
                // std::cout<< "step M1 " << op.target1 << endl;
                op.target1 = -1;
                while (op.target1 < 0) {
                    op.target2 = rand() % (vbuf_.size() - 1);
                    //如果是非叶节点，非法
                    if (vbuf_[op.target2]->type == combine_type::LEAF) {
                        //找左边的相邻叶节点，找不到则非法
                        for (op.target1 = op.target2 - 1; op.target1 >= 0; op.target1--) {
                            if (vbuf_[op.target1]->type == combine_type::LEAF)
                                break;
                        }
                    }
                }
                vtree_.swap_nodes(vbuf_[op.target1], vbuf_[op.target2]);
                std::swap(vbuf_[op.target1], vbuf_[op.target2]);
            } else if (op.type == operation_type::M2) {
                // std::cout<< "step M2 " << op.target1 << endl;
                //如果是叶节点，非法
                while (vbuf_[op.target1]->type == combine_type::LEAF) {
                    op.target1 = rand() % (vbuf_.size() - 1);
                }
                vtree_.invert_chain(vbuf_[op.target1]);
            } else if (op.type == operation_type::M3) {
                // std::cout<< "step M3 " << op.target1 << endl;
                bool valid = false;
                while (!valid) {
                    op.target2 = (rand() % (vbuf_.size() - 2)) + 1;
                    op.target1 = op.target2 - 1;
                    if ((vbuf_[op.target1]->type == combine_type::LEAF) xor
                        (vbuf_[op.target2]->type == combine_type::LEAF)) {
                        valid = vtree_.swap_nodes(vbuf_[op.target1], vbuf_[op.target2]);
                    }
                }
                std::swap(vbuf_[op.target1], vbuf_[op.target2]);
            }
            // std::cout<< "finished" << endl;
            return op;
        }

        struct operation random_operation() {
            struct operation op;
            int tmp = rand() % 3;
            if (tmp == 0) {
                op.type = operation_type::M1;
            } else if (tmp == 1) {
                op.type = operation_type::M2;
            } else {
                op.type = operation_type::M3;
            }
            tmp = rand() % (vbuf_.size() - 1);
            op.target1 = tmp;
            return op;
        }

        template<typename Cont>
        static std::ostream &print_coord_list(Cont &&cont,
            std::ostream &os = std::cout) {
            for (auto &&e : cont)
                cout << "(" << e.first << "," << e.second << ") ";
            return os;
        }

        std::vector<yal::Module> modules_;
        std::vector<expression::polish_expression_type> expr_;
        std::vector<const_iterator> vbuf_;
        vctr_tree_type vtree_, best_tree;
        default_random_engine eng_;
        std::string line_;
        float temperature, cooldown_speed;
        int accept_under_currentT, total_under_currentT;
        float ending_temperature;
        float best_solution;
        int balance_minstep;
    };

}