//
// Created by qlu on 2019/2/11.
//

#include <utility>
#include <algorithm>
#include <fmt/format.h>

#include <kungfu/wingchun/macro.h>

#include "trader_xtp.h"
#include "type_convert_xtp.h"
#include "serialize_xtp.h"

using namespace kungfu::wingchun::msg::data;

namespace kungfu
{
    namespace wingchun
    {
        namespace xtp
        {
            TraderXTP::TraderXTP(bool low_latency, yijinjing::data::locator_ptr locator, const std::string &account_id, const std::string &json_config) :
                    Trader(low_latency, std::move(locator), SOURCE_XTP, account_id), api_(nullptr), session_id_(0), request_id_(0), trading_day_("")
            {
                yijinjing::log::copy_log_settings(get_io_device()->get_home(), account_id);
                config_ = nlohmann::json::parse(json_config);
                order_mapper_ = std::make_shared<OrderMapper>(get_app_db_file("order_mapper"));
            }

            TraderXTP::~TraderXTP()
            {
                if (api_ != nullptr)
                {
                    api_->Release();
                }
            }

            std::string TraderXTP::get_runtime_folder() const
            {
                auto home = get_io_device()->get_home();
                return home->locator->layout_dir(home, yijinjing::data::layout::LOG);
            }

            void TraderXTP::on_start()
            {
                Trader::on_start();
                std::string runtime_folder = get_runtime_folder();
                SPDLOG_INFO("Connecting XTP TD for {} at {}:{} with runtime folder {}", config_.user_id, config_.td_ip, config_.td_port, runtime_folder);
                api_ = XTP::API::TraderApi::CreateTraderApi(config_.client_id, runtime_folder.c_str());
                api_->RegisterSpi(this);
                api_->SubscribePublicTopic(XTP_TERT_QUICK);//只传送登录后公有流（订单响应、成交回报）的内容
                api_->SetSoftwareVersion("1.1.0");
                api_->SetSoftwareKey(config_.software_key.c_str());
                session_id_ = api_->Login(config_.td_ip.c_str(), config_.td_port, config_.user_id.c_str(), config_.password.c_str(), XTP_PROTOCOL_TCP);
                if (session_id_ > 0)
                {
                    publish_state(BrokerState::Ready);
                    LOGIN_INFO("login success");
                    req_account();
                } else
                {
                    publish_state(BrokerState::LoggedInFailed);
                    XTPRI *error_info = api_->GetApiLastError();
                    LOGIN_ERROR(fmt::format("(ErrorId) {}, (ErrorMsg){}", error_info->error_id, error_info->error_msg));
                }
            }

            bool TraderXTP::insert_order(const yijinjing::event_ptr &event)
            {
                const OrderInput &input = event->data<OrderInput>();
                XTPOrderInsertInfo xtp_input = {};
                to_xtp(xtp_input, input);

                // TODO for test only!!!
                xtp_input.order_client_id = 2;

                INSERT_ORDER_TRACE(to_string(xtp_input));

                uint64_t xtp_order_id = api_->InsertOrder(&xtp_input, session_id_);
                int64_t nano = kungfu::yijinjing::time::now_in_nano();
                auto writer = get_writer(event->source());
                msg::data::Order &order = writer->open_data<msg::data::Order>(event->gen_time(), msg::type::Order);
                order_from_input(input, order);
                if (xtp_order_id == 0)
                {
                    order.insert_time = nano;
                    order.update_time = nano;
                    strcpy(order.trading_day, trading_day_.c_str());

                    XTPRI *error_info = api_->GetApiLastError();
                    order.error_id = error_info->error_id;
                    strcpy(order.error_msg, error_info->error_msg);
                    order.status = OrderStatus::Error;

                    writer->close_data();

                    INSERT_ORDER_ERROR(
                            fmt::format("(input){}(ErrorId){}, (ErrorMsg){}", to_string(xtp_input), error_info->error_id, error_info->error_msg));
                    return false;
                } else
                {
                    writer->close_data();

                    XtpOrder info = {};
                    info.internal_order_id = input.order_id;
                    info.xtp_order_id = xtp_order_id;
                    info.parent_id = input.parent_id;
                    info.source = event->source();
                    info.insert_time = nano;
                    strcpy(info.trading_day, trading_day_.c_str());
                    order_mapper_->add_order(info);

                    INSERT_ORDER_TRACE(fmt::format("success to insert order, (order_id){} (xtp_order_id) {}", input.order_id, xtp_order_id));
                    return true;
                }
            }

            bool TraderXTP::cancel_order(const yijinjing::event_ptr &event)
            {
                const OrderAction &action = event->data<OrderAction>();
                uint64_t xtp_order_id = order_mapper_->get_xtp_order_id(action.order_id);
                if (xtp_order_id != 0)
                {
                    auto xtp_action_id = api_->CancelOrder(xtp_order_id, session_id_);
                    if (xtp_action_id == 0)
                    {
                        XTPRI *error_info = api_->GetApiLastError();
                        CANCEL_ORDER_ERROR(fmt::format("(order_xtp_id) {} (session_id) {} (error_id) {} (error_msg) {}", xtp_order_id, session_id_,
                                                       error_info->error_id, error_info->error_msg));
                        return false;
                    } else
                    {
                        CANCEL_ORDER_TRACE(
                                fmt::format("success to request cancel order, (order_id){}, (xtp_order_id){}, (xtp_action_id){}", action.order_id,
                                            xtp_order_id, xtp_action_id));
                        return true;
                    }
                } else
                {
                    CANCEL_ORDER_TRACE(fmt::format("fail to cancel order, can't find related xtp order id of order_id {}", action.order_id));
                    return false;
                }
            }

            void TraderXTP::on_trading_day(const yijinjing::event_ptr &event, int64_t daytime)
            {
                this->trading_day_ = yijinjing::time::strftime(daytime, "%Y%m%d");
            }

            bool TraderXTP::req_position()
            {
                int rtn = api_->QueryPosition(nullptr, this->session_id_, ++request_id_);
                return rtn == 0;
            }

            bool TraderXTP::req_account()
            {

                int rtn = api_->QueryAsset(session_id_, ++request_id_);
                return rtn == 0;
            }

            void TraderXTP::OnDisconnected(uint64_t session_id, int reason)
            {
                if (session_id == session_id_)
                {
                    publish_state(BrokerState::DisConnected);
                    DISCONNECTED_ERROR(fmt::format("(reason) {}", reason));
                }
            }

            void TraderXTP::OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id)
            {
                if (order_info != nullptr)
                {
                    ORDER_TRACE(fmt::format("(xtp_order_info) {} (session_id){}", to_string(*order_info), session_id));
                }
                if (error_info != nullptr)
                {
                    ORDER_ERROR(fmt::format("(error_id){} (error_msg){} (session_id)", error_info->error_id, error_info->error_msg, session_id));
                }
                XtpOrder xtp_order = order_mapper_->get_order_by_xtp_order_id(trading_day_.c_str(), order_info->order_xtp_id);
                if (xtp_order.internal_order_id == 0)
                {
                    ORDER_ERROR(fmt::format("unrecognized xtp_order_id: {} @ trading_day: {}", order_info->order_xtp_id, trading_day_));
                }
                else
                {
                    auto writer = get_writer(xtp_order.source);
                    msg::data::Order &order = writer->open_data<msg::data::Order>(0, msg::type::Order);

                    from_xtp(*order_info, order);
                    order.order_id = xtp_order.internal_order_id;
                    order.parent_id = xtp_order.parent_id;
                    order.insert_time = xtp_order.insert_time;
                    order.update_time = kungfu::yijinjing::time::now_in_nano();
                    strcpy(order.account_id, get_account_id().c_str());
                    strcpy(order.trading_day, xtp_order.trading_day);
                    order.instrument_type = get_instrument_type(order.instrument_id, order.exchange_id);
                    if (error_info != nullptr)
                    {
                        order.error_id = error_info->error_id;
                        strncpy(order.error_msg, error_info->error_msg, ERROR_MSG_LEN);
                    }
                    writer->close_data();
                }
            }

            void TraderXTP::OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id)
            {
                TRADE_TRACE(fmt::format("(trade_info) {}", to_string(*trade_info)));
                XtpOrder xtp_order = order_mapper_->get_order_by_xtp_order_id(trading_day_.c_str(), trade_info->order_xtp_id);
                if (xtp_order.internal_order_id == 0)
                {
                    TRADE_ERROR(fmt::format("unrecognized xtp_order_id {} @trading_day: {}", trade_info->order_xtp_id, trading_day_));
                } else
                {
                    auto writer = get_writer(xtp_order.source);
                    msg::data::Trade &trade = writer->open_data<msg::data::Trade>(0, msg::type::Trade);
                    from_xtp(*trade_info, trade);
                    trade.trade_id = writer->current_frame_uid();
                    trade.order_id = xtp_order.internal_order_id;
                    trade.parent_order_id = xtp_order.parent_id;
                    trade.trade_time = kungfu::yijinjing::time::now_in_nano();
                    strcpy(trade.trading_day, trading_day_.c_str());
                    strcpy(trade.account_id, this->get_account_id().c_str());
                    trade.instrument_type = get_instrument_type(trade.instrument_id, trade.exchange_id);
                    writer->close_data();
                }
            }

            void TraderXTP::OnCancelOrderError(XTPOrderCancelInfo *cancel_info, XTPRI *error_info, uint64_t session_id)
            {
                CANCEL_ORDER_ERROR(fmt::format("(cancel_info){}, (error_id){}, (error_msg){}, (session_id){}", to_string(*cancel_info),
                        error_info->error_id, error_info->error_msg, session_id));
            }

            void TraderXTP::OnQueryPosition(XTPQueryStkPositionRsp *position, XTPRI *error_info, int request_id, bool is_last, uint64_t session_id)
            {
                if (position != nullptr)
                {
                    POSITION_TRACE(fmt::format("(position){} (request_id) {} (last) {}", to_string(*position), request_id, is_last));
                }
                if (error_info != nullptr)
                {
                    POSITION_TRACE(fmt::format("(error_id){} (error_msg) {} (request_id) {} (last) {}", error_info->error_id, error_info->error_msg, request_id, is_last));
                }
                if (error_info == nullptr || error_info->error_id == 0 || error_info->error_id == 11000350)
                {
                    auto writer = get_writer(0);
                    msg::data::Position &stock_pos = writer->open_data<msg::data::Position>(0, msg::type::Position);
                    strcpy(stock_pos.account_id, get_account_id().c_str());
                    if (error_info == nullptr || error_info->error_id == 0)
                    {
                        from_xtp(*position, stock_pos);
                    }
                    stock_pos.instrument_type = get_instrument_type(stock_pos.instrument_id, stock_pos.exchange_id);
                    stock_pos.direction = Direction::Long;
                    strcpy(stock_pos.trading_day, this->trading_day_.c_str());
                    stock_pos.update_time = kungfu::yijinjing::time::now_in_nano();
                    writer->close_data();

                    if (is_last)
                    {
                        writer->mark(0, msg::type::PositionEnd);
                    }
                }
            }

            void TraderXTP::OnQueryAsset(XTPQueryAssetRsp *asset, XTPRI *error_info, int request_id, bool is_last, uint64_t session_id)
            {
                if (asset != nullptr)
                {
                    ACCOUNT_TRACE(fmt::format("(asset){} (request_id){} (last){}", to_string(*asset), request_id, is_last));
                }
                if (error_info != nullptr)
                {
                    ACCOUNT_TRACE(fmt::format("(error_id){} (error_msg) {} (request_id) {} (last) {}", error_info->error_id, error_info->error_msg, request_id, is_last));
                }
                if (error_info == nullptr || error_info->error_id == 0 || error_info->error_id == 11000350)
                {
                    auto writer = get_writer(0);
                    msg::data::Asset &account = writer->open_data<msg::data::Asset>(0, msg::type::Asset);
                    strcpy(account.account_id, get_account_id().c_str());
                    if (error_info == nullptr || error_info->error_id == 0)
                    {
                        from_xtp(*asset, account);
                    }
                    account.update_time = kungfu::yijinjing::time::now_in_nano();
                    writer->close_data();
                    req_position();
                }
            }
        }
    }
}
