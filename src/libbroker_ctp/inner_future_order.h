// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

namespace co {
    /**
     * �ڲ�ί��,����ί�еĳɽ�״̬, ���ڼ����Զ���ƽ�ֵķ���
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
    int64_t order_volume_ = 0;  // ί������
    int64_t match_volume_ = 0;  // �ɽ�����
    int64_t withdraw_volume_ = 0;  // ��������(�ϵ������,��Ϊ��ȫ������)
};
typedef std::shared_ptr<InnerFutureOrder> InnerFutureOrderPtr;
}  // namespace co
