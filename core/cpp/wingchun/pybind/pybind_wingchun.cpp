/*****************************************************************************
 * Copyright [2019] [taurus.ai]
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <kungfu/yijinjing/common.h>
#include <kungfu/wingchun/msg.h>
#include <kungfu/wingchun/common.h>
#include <kungfu/wingchun/commander.h>
#include <kungfu/wingchun/service/ledger.h>
#include <kungfu/wingchun/strategy/context.h>
#include <kungfu/wingchun/strategy/runner.h>

namespace py = pybind11;

using namespace kungfu::yijinjing;
using namespace kungfu::wingchun;
using namespace kungfu::wingchun::service;
using namespace kungfu::wingchun::msg::data;

class PyLedger : public Ledger
{
public:
    using Ledger::Ledger;

    std::string handle_request(const event_ptr &event, const std::string &msg) override
    {PYBIND11_OVERLOAD_PURE(std::string, Ledger, handle_request, event, msg) }

    void on_trader_started(int64_t trigger_time, const kungfu::yijinjing::data::location_ptr &app_location) override
    { PYBIND11_OVERLOAD_PURE(void, Ledger, on_trader_started, trigger_time, app_location) }

    void on_quote(event_ptr event, const Quote &quote) override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, on_quote, event, quote) }

    void on_order(event_ptr event, const Order &order) override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, on_order, event, order) }

    void on_trade(event_ptr event, const Trade &trade) override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, on_trade, event, trade) }

    void on_stock_account(const Asset &asset_info, const std::vector<Position> &positions) override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, on_stock_account, asset_info, positions) }

    void on_future_account(const Asset &asset_info, const std::vector<PositionDetail> &position_details) override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, on_future_account, asset_info, position_details) }

    void on_instruments(const std::vector<Instrument> &instruments) override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, on_instruments, instruments) }

    void on_trading_day(const event_ptr &event, int64_t daytime) override
    {PYBIND11_OVERLOAD(void, Ledger, on_trading_day, event, daytime); }

    void pre_start() override
    {PYBIND11_OVERLOAD_PURE(void, Ledger, pre_start) }
};

class PyRunner : public strategy::Runner
{
public:
    using strategy::Runner::Runner;

    void on_trading_day(const event_ptr &event, int64_t daytime) override
    {PYBIND11_OVERLOAD(void, strategy::Runner, on_trading_day, event, daytime); }
};

class PyStrategy : public strategy::Strategy
{
public:
    using strategy::Strategy::Strategy; // Inherit constructors

    void pre_start(strategy::Context_ptr context) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, pre_start, context); }

    void post_start(strategy::Context_ptr context) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, post_start, context); }

    void pre_stop(strategy::Context_ptr context) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, pre_stop, context); }

    void post_stop(strategy::Context_ptr context) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, post_stop, context); }

    void on_trading_day(strategy::Context_ptr context, int64_t daytime) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, on_trading_day, context, daytime); }

    void on_quote(strategy::Context_ptr context, const Quote &quote) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, on_quote, context, quote); }

    void on_entrust(strategy::Context_ptr context, const Entrust &entrust) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, on_entrust, context, entrust); }

    void on_transaction(strategy::Context_ptr context, const Transaction &transaction) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, on_transaction, context, transaction); }

    void on_order(strategy::Context_ptr context, const Order &order) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, on_order, context, order); }

    void on_trade(strategy::Context_ptr context, const Trade &trade) override
    {PYBIND11_OVERLOAD(void, strategy::Strategy, on_trade, context, trade); }

};

Quote get_quote(journal::frame_ptr frame)
{
    return frame->data<Quote>();
}

PYBIND11_MODULE(pywingchun, m)
{
    auto m_utils = m.def_submodule("utils");
    m_utils.def("get_symbol_id", &kungfu::wingchun::get_symbol_id);
    m_utils.def("is_valid_price", &kungfu::wingchun::is_valid_price);
    m_utils.def("is_final_status", &kungfu::wingchun::is_final_status);
    m_utils.def("get_instrument_type", &kungfu::wingchun::get_instrument_type);
    m_utils.def("json_from_address", &json_from_address);

    m_utils.def("get_quote", get_quote);

    auto m_constants = m.def_submodule("constants");

    py::enum_<kungfu::wingchun::LedgerCategory>(m_constants, "LedgerCategory", py::arithmetic())
            .value("Account", kungfu::wingchun::LedgerCategory::Account)
            .value("Portfolio", kungfu::wingchun::LedgerCategory::Portfolio)
            .value("SubPortfolio", kungfu::wingchun::LedgerCategory::SubPortfolio)
            .value("Unknown", kungfu::wingchun::LedgerCategory::Unknown)            
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::LedgerCategory &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::InstrumentType>(m_constants, "InstrumentType", py::arithmetic())
            .value("Unknown", kungfu::wingchun::InstrumentType::Unknown)
            .value("Stock", kungfu::wingchun::InstrumentType::Stock)
            .value("Future", kungfu::wingchun::InstrumentType::Future)
            .value("Bond", kungfu::wingchun::InstrumentType::Bond)
            .value("StockOption", kungfu::wingchun::InstrumentType::StockOption)
            .value("Fund", kungfu::wingchun::InstrumentType::Fund)
            .value("TechStock", kungfu::wingchun::InstrumentType::TechStock)
            .value("Index", kungfu::wingchun::InstrumentType::Index)
            .value("Repo", kungfu::wingchun::InstrumentType::Repo)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::InstrumentType &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::ExecType>(m_constants, "ExecType", py::arithmetic())
            .value("Unknown", kungfu::wingchun::ExecType::Unknown)
            .value("Cancel", kungfu::wingchun::ExecType::Cancel)
            .value("Trade", kungfu::wingchun::ExecType::Trade)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::ExecType &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::Side>(m_constants, "Side", py::arithmetic())
            .value("Buy", kungfu::wingchun::Side::Buy)
            .value("Sell", kungfu::wingchun::Side::Sell)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::Side &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::Offset>(m_constants, "Offset", py::arithmetic())
            .value("Open", kungfu::wingchun::Offset::Open)
            .value("Close", kungfu::wingchun::Offset::Close)
            .value("CloseToday", kungfu::wingchun::Offset::CloseToday)
            .value("CloseYesterday", kungfu::wingchun::Offset::CloseYesterday)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::Offset &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::OrderStatus>(m_constants, "OrderStatus", py::arithmetic())
            .value("Unknown", kungfu::wingchun::OrderStatus::Unknown)
            .value("Submitted", kungfu::wingchun::OrderStatus::Submitted)
            .value("Pending", kungfu::wingchun::OrderStatus::Pending)
            .value("Cancelled", kungfu::wingchun::OrderStatus::Cancelled)
            .value("Error", kungfu::wingchun::OrderStatus::Error)
            .value("Filled", kungfu::wingchun::OrderStatus::Filled)
            .value("PartialFilledNotActive", kungfu::wingchun::OrderStatus::PartialFilledNotActive)
            .value("PartialFilledActive", kungfu::wingchun::OrderStatus::PartialFilledActive)     
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::OrderStatus &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::Direction>(m_constants, "Direction", py::arithmetic())
            .value("Long", kungfu::wingchun::Direction::Long)
            .value("Short", kungfu::wingchun::Direction::Short)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::Direction &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::PriceType>(m_constants, "PriceType", py::arithmetic())
            .value("Any", kungfu::wingchun::PriceType::Any)
            .value("FakBest5", kungfu::wingchun::PriceType::FakBest5)
            .value("Fak", kungfu::wingchun::PriceType::Fak)
            .value("Fok", kungfu::wingchun::PriceType::Fok)
            .value("Limit", kungfu::wingchun::PriceType::Limit)
            .value("ForwardBest", kungfu::wingchun::PriceType::ForwardBest)
            .value("ReverseBest", kungfu::wingchun::PriceType::ReverseBest)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::PriceType &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::VolumeCondition>(m_constants, "VolumeCondition", py::arithmetic())
            .value("Any", kungfu::wingchun::VolumeCondition::Any)
            .value("Min", kungfu::wingchun::VolumeCondition::Min)
            .value("All", kungfu::wingchun::VolumeCondition::All)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::VolumeCondition &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::TimeCondition>(m_constants, "TimeCondition", py::arithmetic())
            .value("IOC", kungfu::wingchun::TimeCondition::IOC)
            .value("GFD", kungfu::wingchun::TimeCondition::GFD)
            .value("GTC", kungfu::wingchun::TimeCondition::GTC)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::TimeCondition &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::enum_<kungfu::wingchun::msg::type::MsgType>(m_constants, "MsgType", py::arithmetic())
            .value("Error", kungfu::wingchun::msg::type::MsgType::Error)
            .value("Quote", kungfu::wingchun::msg::type::MsgType::Quote)
            .value("Entrust", kungfu::wingchun::msg::type::MsgType::Entrust)
            .value("Transaction", kungfu::wingchun::msg::type::MsgType::Transaction)
            .value("OrderInput", kungfu::wingchun::msg::type::MsgType::OrderInput)
            .value("OrderAction", kungfu::wingchun::msg::type::MsgType::OrderAction)
            .value("Order", kungfu::wingchun::msg::type::MsgType::Order)
            .value("Trade", kungfu::wingchun::msg::type::MsgType::Trade)
            .value("Position", kungfu::wingchun::msg::type::MsgType::Position)
            .value("Asset", kungfu::wingchun::msg::type::MsgType::Asset)
            .value("AssetSnapshot", kungfu::wingchun::msg::type::AssetSnapshot)
            .value("PositionDetail", kungfu::wingchun::msg::type::MsgType::PositionDetail)
            .value("Subscribe", kungfu::wingchun::msg::type::MsgType::Subscribe)
            .value("BrokerState", kungfu::wingchun::msg::type::MsgType::BrokerState)
            .value("PositionEnd", kungfu::wingchun::msg::type::MsgType::PositionEnd)
            .value("PositionDetailEnd", kungfu::wingchun::msg::type::MsgType::PositionDetailEnd)
            .export_values()
            .def("__eq__",
                [](const kungfu::wingchun::msg::type::MsgType &a, int b)
                {
                    return static_cast<int>(a) == b;
                }
            );

    py::class_<Instrument>(m, "Instrument")
            .def_readonly("instrument_type", &Instrument::instrument_type)
            .def_property_readonly("instrument_id", &Instrument::get_instrument_id)
            .def_property_readonly("exchange_id", &Instrument::get_exchange_id)
            .def_readonly("contract_multiplier", &Instrument::contract_multiplier)
            .def_readonly("price_tick", &Instrument::price_tick)
            .def_readonly("delivery_year", &Instrument::delivery_year)
            .def_readonly("delivery_month", &Instrument::delivery_month)
            .def_readonly("is_trading", &Instrument::is_trading)
            .def_readonly("long_margin_ratio", &Instrument::long_margin_ratio)
            .def_readonly("short_margin_ratio", &Instrument::short_margin_ratio)
            .def_property_readonly("product_id", &Instrument::get_product_id)
            .def_property_readonly("open_date", &Instrument::get_open_date)
            .def_property_readonly("create_date", &Instrument::get_create_date)
            .def_property_readonly("expire_date", &Instrument::get_expire_date)            
            .def("__hash__",
                 [](const Instrument &a)
                 {
                     return get_symbol_id(a.get_instrument_id(), a.get_exchange_id());
                 }
            )
            .def("__eq__",
                [](const Instrument &a, const Instrument &b)
                {
                    return strcmp(a.instrument_id, b.instrument_id) == 0 && strcmp(a.exchange_id, b.exchange_id) == 0;
                }
            )
            .def("__repr__",
                 [](const Instrument &a)
                 {
                     return to_string(a);
                 }
            );

    py::class_<Quote>(m, "Quote")
            .def(py::init<>())
            .def_property_readonly("source_id", &Quote::get_source_id)
            .def_property_readonly("trading_day", &Quote::get_trading_day)
            .def("set_source_id", &Quote::set_source_id)
            .def("set_trading_day", &Quote::set_trading_day)
            .def_readwrite("data_time", &Quote::data_time)
            .def_property_readonly("instrument_id", &Quote::get_instrument_id)
            .def_property_readonly("exchange_id", &Quote::get_exchange_id)
            .def("set_instrument_id", &Quote::set_instrument_id)
            .def("set_exchange_id", &Quote::set_exchange_id)
            .def_readwrite("instrument_type", &Quote::instrument_type)
            .def_readwrite("pre_close_price", &Quote::pre_close_price)
            .def_readwrite("pre_settlement_price", &Quote::pre_settlement_price)
            .def_readwrite("last_price", &Quote::last_price)
            .def_readwrite("volume", &Quote::volume)
            .def_readwrite("turnover", &Quote::turnover)
            .def_readwrite("pre_open_interest", &Quote::pre_open_interest)
            .def_readwrite("open_interest", &Quote::open_interest)
            .def_readwrite("open_price", &Quote::open_price)
            .def_readwrite("high_price", &Quote::high_price)
            .def_readwrite("low_price", &Quote::low_price)
            .def_readwrite("upper_limit_price", &Quote::upper_limit_price)
            .def_readwrite("lower_limit_price", &Quote::lower_limit_price)
            .def_readwrite("close_price", &Quote::close_price)
            .def_readwrite("settlement_price", &Quote::settlement_price)
            .def_property_readonly("bid_price", &Quote::get_bid_price)
            .def_property_readonly("ask_price", &Quote::get_ask_price)
            .def_property_readonly("bid_volume", &Quote::get_bid_volume)
            .def_property_readonly("ask_volume", &Quote::get_ask_volume)
            .def("set_bid_price", &Quote::set_bid_price)
            .def("set_ask_price", &Quote::set_ask_price)
            .def("set_bid_volume", &Quote::set_bid_volume)
            .def("set_ask_volume", &Quote::set_ask_volume)
            .def("__repr__",
                 [](const Quote &a)
                 {
                     return to_string(a);
                 })
            .def("__write__",
                 [](const Quote &a, int64_t gen_time, const journal::writer_ptr w)
                 {
                     w->write_with_time(gen_time, kungfu::wingchun::msg::type::Quote, a);
                 }
            );

    py::class_<Entrust>(m, "Entrust")
            .def_property_readonly("source_id", &Entrust::get_source_id)
            .def_property_readonly("trading_day", &Entrust::get_trading_day)
            .def_readonly("data_time", &Entrust::data_time)
            .def_property_readonly("instrument_id", &Entrust::get_instrument_id)
            .def_property_readonly("exchange_id", &Entrust::get_exchange_id)
            .def_readonly("instrument_type", &Entrust::instrument_type)
            .def_readonly("price", &Entrust::price)
            .def_readonly("volume", &Entrust::volume)
            .def_readonly("side", &Entrust::side)
            .def_readonly("price_type", &Entrust::price_type)
            .def_readonly("main_seq", &Entrust::main_seq)
            .def_readonly("seq", &Entrust::seq)
            .def("__repr__",
                 [](const Entrust &a)
                 {
                     return to_string(a);
                 }
            );
    py::class_<Transaction>(m, "Transaction")
            .def_property_readonly("source_id", &Transaction::get_source_id)
            .def_property_readonly("trading_day", &Transaction::get_trading_day)
            .def_readonly("data_time", &Transaction::data_time)
            .def_property_readonly("instrument_id", &Transaction::get_instrument_id)
            .def_property_readonly("exchange_id", &Transaction::get_exchange_id)
            .def_readonly("instrument_type", &Transaction::instrument_type)
            .def_readonly("price", &Transaction::price)
            .def_readonly("volume", &Transaction::volume)
            .def_readonly("bid_no", &Transaction::bid_no)
            .def_readonly("ask_no", &Transaction::ask_no)
            .def_readonly("exec_type", &Transaction::exec_type)
            .def_readonly("bs_flag", &Transaction::bs_flag)
            .def_readonly("main_seq", &Transaction::main_seq)
            .def_readonly("seq", &Transaction::seq)
            .def("__repr__",
                 [](const Transaction &a)
                 {
                     return to_string(a);
                 }
            );

    py::class_<OrderInput>(m, "OrderInput")
            .def(py::init<>())
            .def_readonly("order_id", &OrderInput::order_id)
            .def_readwrite("instrument_type", &OrderInput::instrument_type)
            .def_readwrite("limit_price", &OrderInput::limit_price)
            .def_readwrite("frozen_price", &OrderInput::frozen_price)
            .def_readwrite("volume", &OrderInput::volume)
            .def_readwrite("side", &OrderInput::side)
            .def_readwrite("offset", &OrderInput::offset)
            .def_readwrite("price_type", &OrderInput::price_type)
            .def_readwrite("volume_condition", &OrderInput::volume_condition)
            .def_readwrite("time_condition", &OrderInput::time_condition)
            .def_readwrite("parent_id", &OrderInput::parent_id)
            .def_property("instrument_id", &OrderInput::get_instrument_id, &OrderInput::set_instrument_id)
            .def_property("exchange_id", &OrderInput::get_exchange_id, &OrderInput::set_exchange_id)
            .def_property("account_id", &OrderInput::get_account_id, &OrderInput::set_account_id)
            .def("__repr__",
                 [](const OrderInput &a)
                 {
                     return to_string(a);
                 }
            );

    py::class_<Order>(m, "Order")
            .def(py::init<>())
            .def_readonly("order_id", &Order::order_id)
            .def_readonly("insert_time", &Order::insert_time)
            .def_readonly("update_time", &Order::update_time)
            .def_readonly("instrument_type", &Order::instrument_type)
            .def_readonly("limit_price", &Order::limit_price)
            .def_readonly("frozen_price", &Order::frozen_price)
            .def_readonly("volume", &Order::volume)
            .def_readonly("volume_traded", &Order::volume_traded)
            .def_readonly("volume_left", &Order::volume_left)
            .def_readonly("tax", &Order::tax)
            .def_readonly("commission", &Order::commission)
            .def_readonly("status", &Order::status)
            .def_readonly("error_id", &Order::error_id)
            .def_readonly("side", &Order::side)
            .def_readonly("offset", &Order::offset)
            .def_readonly("price_type", &Order::price_type)
            .def_readonly("volume_condition", &Order::volume_condition)
            .def_readonly("time_condition", &Order::time_condition)
            .def_readonly("parent_id", &Order::parent_id)
            .def_property_readonly("trading_day", &Order::get_trading_day)
            .def_property_readonly("instrument_id", &Order::get_instrument_id)
            .def_property_readonly("exchange_id", &Order::get_exchange_id)
            .def_property_readonly("account_id", &Order::get_account_id)
            .def_property_readonly("client_id", &Order::get_client_id)
            .def_property_readonly("error_msg", &Order::get_error_msg)
            .def("__repr__",
                 [](const Order &a)
                 {
                     return to_string(a);
                 }
            );
    py::class_<OrderAction>(m, "OrderAction")
            .def(py::init<>())
            .def_readonly("order_id", &OrderAction::order_id)
            .def_readonly("order_action_id", &OrderAction::order_action_id)
            .def_readonly("action_flag", &OrderAction::action_flag)
            .def_readonly("price", &OrderAction::price)
            .def_readonly("volume", &OrderAction::volume)
            .def("__repr__",
                 [](const OrderAction &a)
                 {
                     return to_string(a);
                 }
            );
    py::class_<Trade>(m, "Trade")
            .def(py::init<>())
            .def_readonly("trade_id", &Trade::trade_id)
            .def_readonly("order_id", &Trade::order_id)
            .def_readonly("parent_order_id", &Trade::parent_order_id)
            .def_readonly("trade_time", &Trade::trade_time)
            .def_readonly("instrument_type", &Trade::instrument_type)
            .def_readonly("side", &Trade::side)
            .def_readonly("offset", &Trade::offset)
            .def_readonly("price", &Trade::price)
            .def_readonly("volume", &Trade::volume)
            .def_readonly("tax", &Trade::tax)
            .def_readonly("commission", &Trade::commission)
            .def_property_readonly("trading_day", &Trade::get_trading_day)
            .def_property_readonly("instrument_id", &Trade::get_instrument_id)
            .def_property_readonly("exchange_id", &Trade::get_exchange_id)
            .def_property_readonly("account_id", &Trade::get_account_id)
            .def_property_readonly("client_id", &Trade::get_client_id)
            .def("__repr__",
                 [](const Trade &a)
                 {
                     return to_string(a);
                 }
            );

    py::class_<Asset>(m, "Asset")
            .def_readonly("update_time", &Asset::update_time)
            .def_readonly("initial_equity", &Asset::initial_equity)
            .def_readonly("static_equity", &Asset::static_equity)
            .def_readonly("dynamic_equity", &Asset::dynamic_equity)
            .def_readonly("realized_pnl", &Asset::realized_pnl)
            .def_readonly("unrealized_pnl", &Asset::unrealized_pnl)
            .def_readonly("avail", &Asset::avail)
            .def_readonly("market_value", &Asset::market_value)
            .def_readonly("margin", &Asset::margin)
            .def_readonly("accumulated_fee", &Asset::accumulated_fee)
            .def_readonly("intraday_fee", &Asset::intraday_fee)
            .def_readonly("frozen_cash", &Asset::frozen_cash)
            .def_readonly("frozen_margin", &Asset::frozen_margin)
            .def_readonly("frozen_fee", &Asset::frozen_fee)
            .def_readonly("position_pnl", &Asset::position_pnl)
            .def_readonly("close_pnl", &Asset::close_pnl)
            .def_property_readonly("trading_day", &Asset::get_trading_day)
            .def_property_readonly("account_id", &Asset::get_account_id)
            .def_property_readonly("broker_id", &Asset::get_broker_id)
            .def_property_readonly("source_id", &Asset::get_source_id)
            .def("__repr__",
                 [](const Asset &a)
                 {
                     return to_string(a);
                 }
            );

    py::class_<Position>(m, "Position")
            .def_readonly("update_time", &Position::update_time)
            .def_readonly("instrument_type", &Position::instrument_type)
            .def_readonly("direction", &Position::direction)
            .def_readonly("volume", &Position::volume)
            .def_readonly("yesterday_volume", &Position::yesterday_volume)
            .def_readonly("frozen_total", &Position::frozen_total)
            .def_readonly("frozen_yesterday", &Position::frozen_yesterday)
            .def_readonly("last_price", &Position::last_price)
            .def_readonly("avg_open_price", &Position::avg_open_price)
            .def_readonly("position_cost_price", &Position::position_cost_price)
            .def_readonly("close_price", &Position::close_price)
            .def_readonly("pre_close_price", &Position::pre_close_price)
            .def_readonly("settlement_price", &Position::settlement_price)
            .def_readonly("pre_settlement_price", &Position::pre_settlement_price)
            .def_readonly("margin", &Position::margin)
            .def_readonly("position_pnl", &Position::position_pnl)
            .def_readonly("close_pnl", &Position::close_pnl)
            .def_readonly("realized_pnl", &Position::realized_pnl)
            .def_readonly("unrealized_pnl", &Position::unrealized_pnl)
            .def_property_readonly("trading_day", &Position::get_trading_day)
            .def_property_readonly("instrument_id", &Position::get_instrument_id)
            .def_property_readonly("exchange_id", &Position::get_exchange_id)
            .def_property_readonly("account_id", &Position::get_account_id)
            .def_property_readonly("client_id", &Position::get_client_id)
            .def("__repr__",
                 [](const Position &a)
                 {
                     return to_string(a);
                 }
            );

            py::class_<PositionDetail>(m, "PositionDetail")
            .def_readonly("update_time", &PositionDetail::update_time)
            .def_readonly("instrument_type", &PositionDetail::instrument_type)
            .def_readonly("direction", &PositionDetail::direction)
            .def_readonly("volume", &PositionDetail::volume)
            .def_readonly("frozen_volume", &PositionDetail::frozen_volume)
            .def_readonly("last_price", &PositionDetail::last_price)
            .def_readonly("open_price", &PositionDetail::open_price)
            .def_readonly("settlement_price", &PositionDetail::settlement_price)
            .def_readonly("pre_settlement_price", &PositionDetail::pre_settlement_price)
            .def_readonly("trade_id", &PositionDetail::trade_id)
            .def_readonly("trade_time", &PositionDetail::trade_time)
            .def_property_readonly("trading_day", &PositionDetail::get_trading_day)
            .def_property_readonly("instrument_id", &PositionDetail::get_instrument_id)
            .def_property_readonly("exchange_id", &PositionDetail::get_exchange_id)
            .def_property_readonly("account_id", &PositionDetail::get_account_id)
            .def_property_readonly("client_id", &PositionDetail::get_client_id)
            .def_property_readonly("open_date", &PositionDetail::get_open_date)
            .def("__repr__",
            [](const PositionDetail &a)
            {
                return to_string(a);
            }
            );

    py::class_<MsgWriter, std::shared_ptr<MsgWriter>>(m, "MsgWriter")
            .def(py::init<kungfu::yijinjing::journal::writer_ptr>())
            .def("write_data", &MsgWriter::write_data);
    
    py::class_<Ledger, PyLedger, kungfu::practice::apprentice, std::shared_ptr<Ledger>>(m, "Ledger")
            .def(py::init<data::locator_ptr, data::mode, bool>())
            .def_property_readonly("io_device", &Ledger::get_io_device)
            .def("now", &Ledger::now)
            .def("has_location", &Ledger::has_location)
            .def("get_location", &Ledger::get_location)
            .def("publish", &Ledger::publish)
            .def("publish_broker_states", &Ledger::publish_broker_states)
            .def("new_order_single", &Ledger::new_order_single)
            .def("cancel_order", &Ledger::cancel_order)
            .def("handle_request", &Ledger::handle_request)
            .def("on_trading_day", &Ledger::on_trading_day)
            .def("on_trader_started", &Ledger::on_trader_started)
            .def("on_quote", &Ledger::on_quote)
            .def("on_order", &Ledger::on_order)
            .def("on_trade", &Ledger::on_trade)
            .def("on_stock_account", &Ledger::on_stock_account)
            .def("on_future_account", &Ledger::on_future_account)
            .def("on_instruments", &Ledger::on_instruments)
            .def("set_begin_time", &Ledger::set_begin_time)
            .def("set_end_time", &Ledger::set_end_time)
            .def("pre_start", &Ledger::pre_start)
            .def("add_timer", &Ledger::add_timer)
            .def("add_time_interval", &Ledger::add_time_interval)
            .def("run", &Ledger::run);

    py::class_<strategy::Runner, PyRunner, kungfu::practice::apprentice, std::shared_ptr<strategy::Runner>>(m, "Runner")
            .def(py::init<kungfu::yijinjing::data::locator_ptr, const std::string &, const std::string &, data::mode, bool>())
            .def("set_begin_time", &strategy::Runner::set_begin_time)
            .def("set_end_time", &strategy::Runner::set_end_time)
            .def("run", &strategy::Runner::run)
            .def("on_trading_day", &strategy::Runner::on_trading_day)
            .def("add_strategy", &strategy::Runner::add_strategy);

    py::class_<strategy::Context, std::shared_ptr<strategy::Context>>(m, "Context")
            .def("now", &strategy::Context::now)
            .def("add_timer", &strategy::Context::add_timer)
            .def("add_time_interval", &strategy::Context::add_time_interval)
            .def("add_account", &strategy::Context::add_account)
            .def("list_accounts", &strategy::Context::list_accounts)
            .def("get_account_cash_limit", &strategy::Context::get_account_cash_limit)
            .def("subscribe", &strategy::Context::subscribe)
            .def("insert_order", &strategy::Context::insert_order)
            .def("cancel_order", &strategy::Context::cancel_order);

    py::class_<strategy::Strategy, PyStrategy, strategy::Strategy_ptr>(m, "Strategy")
            .def(py::init())
            .def("pre_start", &strategy::Strategy::pre_start)
            .def("post_start", &strategy::Strategy::post_start)
            .def("pre_stop", &strategy::Strategy::pre_stop)
            .def("post_stop", &strategy::Strategy::post_stop)
            .def("on_trading_day", &strategy::Strategy::on_trading_day)
            .def("on_quote", &strategy::Strategy::on_quote)
            .def("on_entrust", &strategy::Strategy::on_entrust)
            .def("on_transaction", &strategy::Strategy::on_transaction)
            .def("on_order", &strategy::Strategy::on_order)
            .def("on_trade", &strategy::Strategy::on_trade);

}
