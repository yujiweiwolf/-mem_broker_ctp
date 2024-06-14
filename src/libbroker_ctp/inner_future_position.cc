// Copyright 2021 Fancapital Inc.  All rights reserved.
#include "inner_future_position.h"

namespace co {
    InnerFuturePositionPtr InnerFuturePosition::New(string code, int64_t hedge_flag, int64_t bs_flag) {
        return std::make_shared<InnerFuturePosition>(code, hedge_flag, bs_flag);
    }

    InnerFuturePosition::InnerFuturePosition(string code, int64_t hedge_flag, int64_t bs_flag):
        code_(code),
        hedge_flag_(hedge_flag),
        bs_flag_(bs_flag) {
    }

    string InnerFuturePosition::ToString() {
        stringstream ss;
        ss << "InnerPosition{";
        ss << "code: " << code_
            << ", bs_flag: " << bs_flag_
            << ", yd_volume: " << yd_volume_
            << ", yd_closing_volume: " << yd_closing_volume_
            << ", yd_close_volume: " << yd_close_volume_
            << ", td_volume: " << td_volume_
            << ", td_closing_volume: " << td_closing_volume_
            << ", td_close_volume: " << td_close_volume_
            << ", td_opening_volume: " << td_opening_volume_
            << ", td_open_volume: " << td_open_volume_
            << "}";
        return ss.str();
    }
}  // namespace co
