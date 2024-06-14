#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <x/x.h>
#include "mem_broker/mem_base_broker.h"
#include "mem_broker/mem_server.h"

using namespace std;

namespace co {

    class Config {
    public:
        static Config* Instance();

        inline string ctp_trade_front() {
            return ctp_trade_front_;
        }
        inline string ctp_broker_id() {
            return ctp_broker_id_;
        }
        inline string ctp_investor_id() {
            return ctp_investor_id_;
        }
        inline string ctp_password() {
            return ctp_password_;
        }
        inline string ctp_app_id() {
            return ctp_app_id_;
        }
        inline string ctp_product_info() {
            return ctp_product_info_;
        }
        inline string ctp_auth_code() {
            return ctp_auth_code_;
        }

        inline bool risk_forbid_closing_today() {
            return risk_forbid_closing_today_;
        }

        inline int risk_max_today_opening_volume() {
            return risk_max_today_opening_volume_;
        }

        inline MemBrokerOptionsPtr options() {
            return options_;
        }

        inline bool disable_subscribe() {
            return disable_subscribe_;
        }

    protected:
        Config() = default;
        ~Config() = default;
        Config(const Config&) = delete;
        const Config& operator=(const Config&) = delete;

        void Init();

    private:
        static Config* instance_;

        MemBrokerOptionsPtr options_;

        string ctp_trade_front_;

        string ctp_broker_id_;
        string ctp_investor_id_;
        string ctp_password_;
        string ctp_app_id_;
        string ctp_product_info_;
        string ctp_auth_code_;

        bool disable_subscribe_ = false;

        bool risk_forbid_closing_today_ = false;
        int risk_max_today_opening_volume_ = 0;
    };

}