#include "node.h"

node::node(sc_core::sc_module_name name) : m_val(0), m_credit(0), m_credit_ret(0) {
    rx.register_nb_transport_fw(this, &node::nb_transport_fw);
    SC_METHOD(mth_entry);
    sensitive << m_clk.pos();
    dont_initialize();

    m_cycle_cnt = 0;

    get_config("node.xml");

    m_logger = spdlog::basic_logger_mt(
        this->name(), std::string("./logs/") + this->name() + ".log", true);
    m_logger->set_level(spdlog::level::info);
    m_logger->set_pattern("%v");
}

void node::get_config(const char* xml_file) {
    XMLDocument doc;
    XMLError eResult = doc.LoadFile(xml_file);

    if (eResult != XML_SUCCESS) {
        cerr << "Error loading XML file: " << doc.ErrorName() << endl;
    }

    int num = 0;

    XMLElement* root = doc.FirstChildElement("node");
    if (root == nullptr) {
        cerr << "Error: Could not find the root element 'node'." << endl;
    }

    XMLElement* settingsElement = root->FirstChildElement("cfg");
    if (settingsElement) {
        XMLElement* databaseElement = settingsElement->FirstChildElement("credit");
        if (databaseElement) {
            if (databaseElement->QueryAttribute("num", &num) != XML_SUCCESS) {
                cerr << "Error: Could not find the element 'num'." << endl;
            }
        }
    }

    m_credit = num;
}

void node::mth_entry() {
    m_cycle_cnt++;
    cal_speed();

    if (m_credit > 0) {
        auto p_api = new MY_API_T();
        p_api->type = DATA_MSG;

        auto p_dat = std::make_shared<MY_DAT_T>();
        p_dat->a = 10 + m_val;
        p_dat->b = 'a';
        p_dat->c = 101;
        p_api->dat = p_dat;

        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr((unsigned char*)p_api);
        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        sc_core::sc_time time = sc_core::SC_ZERO_TIME;

        tx->nb_transport_fw(trans, phase, time);
        m_logger->info("cycle[{:6d}] snd value: 0x{:x}", m_cycle_cnt, p_dat->a);
        m_credit -= 1;
        m_val++;
    }

    if (m_credit_ret > 0) {
        auto p_api = new MY_API_T();
        p_api->type = CTRL_MSG;

        auto p_ctrl = std::make_shared<MY_CTRL_T>();
        int credit = 1;
        p_ctrl->credit = credit;
        p_api->dat = p_ctrl;

        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr((unsigned char*)p_api);
        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        sc_core::sc_time time = sc_core::SC_ZERO_TIME;

        tx->nb_transport_fw(trans, phase, time);
        m_credit_ret--;
        m_logger->info("cycle[{:6d}] snd credit: {:d}", m_cycle_cnt, credit);
    }
}

void node::cal_speed() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    // std::cout << "当前时间：" << ts.tv_sec << "." << ts.tv_nsec << std::endl;

    double cur_us = sc_core::sc_time_stamp().to_seconds() * 10e6;

    // std::cout << cur_us << std::endl;
}

tlm::tlm_sync_enum node::nb_transport_fw(tlm::tlm_generic_payload& trans,
                                        tlm::tlm_phase& phase,
                                        sc_core::sc_time& time) {
    auto p_api = (MY_API_T*)trans.get_data_ptr();
    auto type = p_api->type;

    if (type == CTRL_MSG) {
        auto p_ctrl = std::static_pointer_cast<MY_CTRL_T>(p_api->dat);
        m_logger->info("cycle[{:6d}] rcv credit: {:d}", m_cycle_cnt, p_ctrl->credit);
        m_credit += p_ctrl->credit;
    } else if (type == DATA_MSG) {
        auto p_dat = std::static_pointer_cast<MY_DAT_T>(p_api->dat);
        m_logger->info("cycle[{:6d}] rcv data: 0x{:x}", m_cycle_cnt, p_dat->a);

        m_credit_ret += 1;
    }

    delete p_api;
    return tlm::TLM_COMPLETED;
}

void node::set_credit(int credit) { m_credit = credit; }