#include "config.h"
#include "yaml-cpp/yaml.h"
namespace co {

    Config* Config::instance_ = 0;

    Config* Config::Instance() {
        if (instance_ == 0) {
            instance_ = new Config();
            instance_->Init();
        }
        return instance_;
    }

    void Config::Init() {
        // 读取配置
        auto getStr = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<std::string>() : "";
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getStrings = [&](std::vector<std::string>* ret, const YAML::Node& node, const std::string& name, bool drop_empty = false) {
            try {
                if (node[name] && !node[name].IsNull()) {
                    for (auto item : node[name]) {
                        std::string s = x::Trim(item.as<std::string>());
                        if (!drop_empty || !s.empty()) {
                            ret->emplace_back(s);
                        }
                    }
                }
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getInt = [&](const YAML::Node& node, const std::string& name, const int64_t& default_value = 0) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<int64_t>() : default_value;
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };
        auto getBool = [&](const YAML::Node& node, const std::string& name) {
            try {
                return node[name] && !node[name].IsNull() ? node[name].as<bool>() : false;
            } catch (std::exception& e) {
                LOG_ERROR << "load configuration failed: name = " << name << ", error = " << e.what();
                throw std::runtime_error(e.what());
            }
        };

        auto filename = x::FindFile("broker.yaml");
        YAML::Node root = YAML::LoadFile(filename);
        options_ = MemBrokerOptions::Load(filename);

        auto broker = root["ctp"];
        ctp_trade_front_ = getStr(broker, "ctp_trade_front");
        ctp_broker_id_ = getStr(broker, "ctp_broker_id");
        ctp_investor_id_ = getStr(broker, "ctp_investor_id");
        string __password_ = getStr(broker, "ctp_password");
        ctp_password_ = DecodePassword(__password_);
        ctp_app_id_ = getStr(broker, "ctp_app_id");
        ctp_product_info_ = getStr(broker, "ctp_product_info");
        ctp_auth_code_ = getStr(broker, "ctp_auth_code");
        disable_subscribe_ = getBool(broker, "disable_subscribe");

        auto risk = root["risk"];
        risk_forbid_closing_today_ = getBool(risk, "risk_forbid_closing_today");
        risk_max_today_opening_volume_ = getInt(risk, "risk_max_today_opening_volume");
        try {
            // string log_level = ini.get<string>("log.level");
            // x::SetLogLevel(log_level);
        } catch (...) {
            // pass
        }
        stringstream ss;
        ss << "+-------------------- configuration begin --------------------+" << endl;
        ss << options_->ToString() << endl;
        ss << endl;
        ss << "ctp:" << endl
            // << "  ctp_market_front: " << ctp_market_front_ << endl
            << "  ctp_trade_front: " << ctp_trade_front_ << endl
            << "  ctp_broker_id: " << ctp_broker_id_ << endl
            << "  ctp_investor_id: " << ctp_investor_id_ << endl
            << "  ctp_password: " << string(ctp_password_.size(), '*') << endl
            << "  ctp_app_id: " << ctp_app_id_ << endl
            << "  ctp_product_info: " << ctp_product_info_ << endl
            << "  ctp_auth_code: " << ctp_auth_code_ << endl
            << "  disable_subscribe: " << (disable_subscribe_ ? "true" : "false") << endl
            << "risk:" << endl
            << "  risk_forbid_closing_today: " << (risk_forbid_closing_today_ ? "true" : "false") << endl
            << "  risk_max_today_opening_volume: " << risk_max_today_opening_volume_ << endl;
        ss << "+-------------------- configuration end   --------------------+";
        LOG_INFO << endl << ss.str();
    }

}