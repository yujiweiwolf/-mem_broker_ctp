// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

namespace co {
    /**
     * 内部委托,保存委托的成交状态, 用于计算自动开平仓的方向
     */
class InnerFutureOrder {
 public:
    inline int64_t order_volume() {
        return order_volume_;
    }
    inline void set_order_volume(int64_t v) {
        order_volume_ = v;
    }
    inline int64_t match_volume() {
        return match_volume_;
    }
    inline void set_match_volume(int64_t v) {
        match_volume_ = v;
    }
    inline int64_t withdraw_volume() {
        return withdraw_volume_;
    }
    inline void set_withdraw_volume(int64_t v) {
        withdraw_volume_ = v;
    }

 private:
    int64_t order_volume_ = 0;  // 委托数量
    int64_t match_volume_ = 0;  // 成交数量
    int64_t withdraw_volume_ = 0;  // 撤单数量(废单情况下,认为是全部撤单)
};
typedef std::shared_ptr<InnerFutureOrder> InnerFutureOrderPtr;
}  // namespace co
