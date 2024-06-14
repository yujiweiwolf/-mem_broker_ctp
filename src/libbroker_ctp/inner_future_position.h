// Copyright 2021 Fancapital Inc.  All rights reserved.
#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

using namespace std;

namespace co {
    /**
     * �ڲ��ֲ֣���<code>_<hedge_flag>_<bs_flag>���л���, ���ڼ����Զ���ƽ�ֵķ���//
     */
class InnerFuturePosition {
 public:
    static std::shared_ptr<InnerFuturePosition> New(string code, int64_t hedge_flag, int64_t bs_flag);

    InnerFuturePosition(string code, int64_t hedge_flag, int64_t bs_flag);

    string ToString();

    inline string code() {
        return code_;
    }
    inline int64_t hedge_flag() {
        return hedge_flag_;
    }
    inline int64_t bs_flag() {
        return bs_flag_;
    }
    inline int64_t yd_volume() {
        return yd_volume_;
    }
    inline void set_yd_volume(int64_t v) {
        yd_volume_ = v;
    }
    inline int64_t yd_closing_volume() {
        return yd_closing_volume_;
    }
    inline void set_yd_closing_volume(int64_t v) {
        yd_closing_volume_ = v;
    }
    inline int64_t yd_close_volume() {
        return yd_close_volume_;
    }
    inline void set_yd_close_volume(int64_t v) {
        yd_close_volume_ = v;
    }
    inline int64_t td_volume() {
        return td_volume_;
    }
    inline void set_td_volume(int64_t v) {
        td_volume_ = v;
    }
    inline int64_t td_closing_volume() {
        return td_closing_volume_;
    }
    inline void set_td_closing_volume(int64_t v) {
        td_closing_volume_ = v;
    }
    inline int64_t td_close_volume() {
        return td_close_volume_;
    }
    inline void set_td_close_volume(int64_t v) {
        td_close_volume_ = v;
    }
    inline int64_t td_opening_volume() {
        return td_opening_volume_;
    }
    inline void set_td_opening_volume(int64_t v) {
        td_opening_volume_ = v;
    }
    inline int64_t td_open_volume() {
        return td_open_volume_;
    }
    inline void set_td_open_volume(int64_t v) {
        td_open_volume_ = v;
    }

 private:
    string code_;
    int64_t hedge_flag_ = 0;  // �ױ���ǣ�1-Ͷ����2-������3-�ױ�
    int64_t bs_flag_ = 0;  // ������ǣ�1-���룬2-����
    int64_t yd_volume_ = 0;  // ���ճֲ�
    int64_t yd_closing_volume_ = 0;  // ���ճֲ�ƽ�ֶ�����
    int64_t yd_close_volume_ = 0;  // ���ճֲ���ƽ����
    int64_t td_volume_ = 0;  // ���ճֲ�
    int64_t td_closing_volume_ = 0;  // ���ճֲ�ƽ�ֶ�����
    int64_t td_close_volume_ = 0;  // ���ճֲ���ƽ����
    int64_t td_opening_volume_ = 0;  // ���ճֲֿ��ֶ�����
    int64_t td_open_volume_ = 0;  // ���ճֲ��ѿ�����
};
typedef std::shared_ptr<InnerFuturePosition> InnerFuturePositionPtr;
}  // namespace co
