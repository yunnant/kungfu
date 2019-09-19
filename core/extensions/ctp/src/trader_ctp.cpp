//
// Created by qlu on 2019/1/14.
//

#include <chrono>

#include <kungfu/wingchun/msg.h>
#include <kungfu/wingchun/macro.h>
#include <kungfu/wingchun/encoding.h>

#include "trader_ctp.h"
#include "serialize_ctp.h"
#include "type_convert_ctp.h"

using namespace kungfu::yijinjing;
using namespace kungfu::wingchun::msg::data;

namespace kungfu
{
    namespace wingchun
    {
        namespace ctp
        {
            TraderCTP::TraderCTP(bool low_latency, data::locator_ptr locator, const std::string &account_id, const std::string &json_config) :
                    Trader(low_latency, std::move(locator), SOURCE_CTP, account_id), front_id_(-1), session_id_(-1),order_ref_(-1), request_id_(0), api_(nullptr)
            {
                yijinjing::log::copy_log_settings(get_io_device()->get_home(), account_id);
                config_ =  nlohmann::json::parse(json_config);
                order_mapper_ = std::make_shared<OrderMapper>(get_app_db_file("order_mapper"));
                trade_mapper_ = std::make_shared<TradeMapper>(get_app_db_file("trade_mapper"));
            }

            void TraderCTP::on_start()
            {
                broker::Trader::on_start();

                if (api_ == nullptr)
                {
                    auto home = get_io_device()->get_home();
                    std::string runtime_folder = home->locator->layout_dir(home, yijinjing::data::layout::LOG);
                    SPDLOG_INFO("create ctp td api with path: {}", runtime_folder);
                    api_ = CThostFtdcTraderApi::CreateFtdcTraderApi(runtime_folder.c_str());
                    api_->RegisterSpi(this);
                    api_->RegisterFront((char *) config_.td_uri.c_str());
                    api_->SubscribePublicTopic(THOST_TERT_QUICK);
                    api_->SubscribePrivateTopic(THOST_TERT_QUICK);
                    api_->Init();
                }
            }

            bool TraderCTP::login()
            {
                CThostFtdcReqUserLoginField login_field = {};
                strcpy(login_field.TradingDay, "");
                strcpy(login_field.UserID, config_.account_id.c_str());
                strcpy(login_field.BrokerID, config_.broker_id.c_str());
                strcpy(login_field.Password, config_.password.c_str());
                LOGIN_TRACE(fmt::format("(UserID) {} (BrokerID) {} (Password) ***", login_field.UserID, login_field.BrokerID));
                int rtn = api_->ReqUserLogin(&login_field, ++request_id_);
                if (rtn != 0)
                {
                    LOGIN_ERROR(fmt::format("failed to request login, (error_id) {}", rtn));
                    return false;
                } else
                {
                    return true;
                }
            }

            bool TraderCTP::req_settlement_confirm()
            {
                CThostFtdcSettlementInfoConfirmField req = {};
                strcpy(req.InvestorID, config_.account_id.c_str());
                strcpy(req.BrokerID, config_.broker_id.c_str());
                int rtn = api_->ReqSettlementInfoConfirm(&req, ++request_id_);
                return rtn == 0;
            }

            bool TraderCTP::req_auth()
            {
                struct CThostFtdcReqAuthenticateField req = {};
                strcpy(req.UserID, config_.account_id.c_str());
                strcpy(req.BrokerID, config_.broker_id.c_str());
                if (config_.product_info.length() > 0)
                {
                    strcpy(req.UserProductInfo, config_.product_info.c_str());
                }
                strcpy(req.AppID, config_.app_id.c_str());
                strcpy(req.AuthCode, config_.auth_code.c_str());
                int rtn = this->api_->ReqAuthenticate(&req, ++request_id_);
                if (rtn != 0)
                {
                    LOGIN_ERROR(fmt::format("failed to req auth, error id = {}", rtn));
                }
                return rtn == 0;
            }

            bool TraderCTP::req_account()
            {
                CThostFtdcQryTradingAccountField req = {};
                strcpy(req.BrokerID, config_.broker_id.c_str());
                strcpy(req.InvestorID, config_.account_id.c_str());
                int rtn = api_->ReqQryTradingAccount(&req, ++request_id_);
                return rtn == 0;
            }

            bool TraderCTP::req_position()
            {
                CThostFtdcQryInvestorPositionField req = {};
                strcpy(req.BrokerID, config_.broker_id.c_str());
                strcpy(req.InvestorID, config_.account_id.c_str());
                int rtn = api_->ReqQryInvestorPosition(&req, ++request_id_);
                return rtn == 0;
            }

            bool TraderCTP::req_position_detail()
            {
                CThostFtdcQryInvestorPositionDetailField req = {};
                strcpy(req.BrokerID, config_.broker_id.c_str());
                strcpy(req.InvestorID, config_.account_id.c_str());
                int rtn = api_->ReqQryInvestorPositionDetail(&req, ++request_id_);
                if (rtn != 0)
                {
                    LOGIN_ERROR("failed to req position detail");
                }

                return rtn == 0;
            }

            bool TraderCTP::req_qry_instrument()
            {
                CThostFtdcQryInstrumentField req = {};
                int rtn = api_->ReqQryInstrument(&req, ++request_id_);
                return rtn == 0;
            }

            bool TraderCTP::insert_order(const yijinjing::event_ptr& event)
            {
                const OrderInput &input = event->data<OrderInput>();

                CThostFtdcInputOrderField ctp_input;
                memset(&ctp_input, 0, sizeof(ctp_input));

                to_ctp(ctp_input, input);
                strcpy(ctp_input.BrokerID, config_.broker_id.c_str());
                strcpy(ctp_input.InvestorID, config_.account_id.c_str());

                int order_ref = order_ref_++;
                strcpy(ctp_input.OrderRef, std::to_string(order_ref).c_str());

                INSERT_ORDER_TRACE(to_string(ctp_input));
                int error_id = api_->ReqOrderInsert(&ctp_input, ++request_id_);
                int64_t nano = kungfu::yijinjing::time::now_in_nano();
                if (error_id != 0)
                {
                    auto writer = get_writer(event->source());
                    msg::data::Order &order = writer->open_data<msg::data::Order>(event->gen_time(), msg::type::Order);
                    order_from_input(input, order);
                    order.insert_time = nano;
                    order.update_time = nano;
                    order.error_id = error_id;
                    order.status = OrderStatus::Error;
                    writer->close_data();
                    INSERT_ORDER_ERROR(fmt::format("(error_id) {}", error_id));
                    return false;
                } else
                {
                    CtpOrder order_record = {};
                    order_record.internal_order_id = input.order_id;
                    order_record.broker_id = config_.broker_id;
                    order_record.investor_id = config_.account_id;
                    order_record.exchange_id = input.exchange_id;
                    order_record.instrument_id = ctp_input.InstrumentID;
                    order_record.order_ref = ctp_input.OrderRef;
                    order_record.front_id = front_id_;
                    order_record.session_id = session_id_;
                    order_record.source = event->source();
                    order_record.parent_id = input.parent_id;
                    order_record.insert_time = nano;
                    order_mapper_->add_order(input.order_id, order_record);
                    INSERT_ORDER_INFO(fmt::format("(FrontID) {} (SessionID) {} (OrderRef) {}", front_id_, session_id_, ctp_input.OrderRef));
                    return true;
                }
            }

            bool TraderCTP::cancel_order(const yijinjing::event_ptr& event)
            {
                const OrderAction &action = event->data<OrderAction>();
                CtpOrder order_record = order_mapper_->get_order_info(action.order_id);
                if (order_record.internal_order_id == 0)
                {
                    SPDLOG_ERROR("failed to find order info for {}", action.order_id);
                    return false;
                }
                CThostFtdcInputOrderActionField ctp_action = {};
                strcpy(ctp_action.BrokerID, order_record.broker_id.c_str());
                strcpy(ctp_action.InvestorID, order_record.investor_id.c_str());
                strcpy(ctp_action.OrderRef, order_record.order_ref.c_str());
                ctp_action.FrontID = order_record.front_id;
                ctp_action.SessionID = order_record.session_id;
                ctp_action.ActionFlag = THOST_FTDC_AF_Delete;
                strcpy(ctp_action.InstrumentID, order_record.instrument_id.c_str());
                strcpy(ctp_action.ExchangeID, order_record.exchange_id.c_str());
                CANCEL_ORDER_TRACE(
                        fmt::format("(FrontID) {} (SessionID) {} (OrderRef)", ctp_action.FrontID, ctp_action.SessionID, ctp_action.OrderRef));

                int error_id = api_->ReqOrderAction(&ctp_action, ++request_id_);
                if (error_id == 0)
                {
                    return true;
                } else
                {
                    CANCEL_ORDER_ERROR(fmt::format("(error_id) {}", error_id));
                    return false;
                }
            }

            void TraderCTP::OnFrontConnected()
            {
                CONNECT_INFO();
                req_auth();
            }

            void TraderCTP::OnFrontDisconnected(int nReason)
            {
                DISCONNECTED_ERROR(fmt::format("(nReason) {} (Info) {}", nReason, disconnected_reason(nReason)));
            }

            void TraderCTP::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField,
                                              CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    LOGIN_ERROR(fmt::format("[OnRspAuthenticate] (ErrorId) {} (ErrorMsg) {}", pRspInfo->ErrorID, pRspInfo->ErrorMsg));
                }
                else
                {
                    LOGIN_INFO("[OnRspAuthenticate]");
                    login();
                }
            }

            void TraderCTP::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo,
                                           int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    LOGIN_ERROR(fmt::format("[OnRspUserLogin] (ErrorId) {} (ErrorMsg) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg)));
                } else
                {
                    LOGIN_INFO(fmt::format("[OnRspUserLogin] (Bid) {} (Uid) {} (SName) {} (TradingDay) {} (FrontID) {} (SessionID) {}",
                                           pRspUserLogin->BrokerID, pRspUserLogin->UserID, pRspUserLogin->SystemName, pRspUserLogin->TradingDay,
                                           pRspUserLogin->FrontID, pRspUserLogin->SessionID));

                    session_id_ = pRspUserLogin->SessionID;
                    front_id_ = pRspUserLogin->FrontID;
                    order_ref_ = atoi(pRspUserLogin->MaxOrderRef);
                    req_settlement_confirm();
                }
            }

            void TraderCTP::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo,
                                            int nRequestID, bool bIsLast)
            {}

            void TraderCTP::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo,
                                                       int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    LOGIN_ERROR(fmt::format("[OnRspSettlementInfoConfirm] (ErrorId) {} (ErrorMsg) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg)));
                } else
                {
                    LOGIN_INFO(fmt::format("[OnRspSettlementInfoConfirm]"));
                    publish_state(BrokerState::Ready);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    req_qry_instrument();
                }
            }

            void TraderCTP::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo,
                                             int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    auto order_info = order_mapper_->get_order_info_by_order_ref(this->front_id_, this->session_id_, pInputOrder->OrderRef);
                    if (order_info.internal_order_id != 0)
                    {
                        OrderInput input = {};
                        from_ctp(*pInputOrder, input);
                        auto writer = get_writer(order_info.source);
                        msg::data::Order &order = writer->open_data<msg::data::Order>(0, msg::type::Order);
                        order.order_id = order_info.internal_order_id;
                        order.parent_id = order_info.parent_id;
                        order.insert_time = order_info.insert_time;
                        order.error_id = pRspInfo->ErrorID;
                        order.update_time = kungfu::yijinjing::time::now_in_nano();
                        strcpy(order.error_msg, gbk2utf8(pRspInfo->ErrorMsg).c_str());
                        order.status = OrderStatus::Error;
                        writer->close_data();
                    }
                    ORDER_ERROR(fmt::format("[OnRspOrderInsert] (ErrorId) {} (ErrorMsg) {}, (InputOrder) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg), pInputOrder == nullptr ? "" : to_string(*pInputOrder)));
                }
            }

            void TraderCTP::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction,
                                             CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    CANCEL_ORDER_ERROR(fmt::format("[OnRspOrderAction] (ErrorId) {} (ErrorMsg) {} (InputOrderAction) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg), pInputOrderAction == nullptr ? "" : to_string(*pInputOrderAction)));
                }
            }

            void TraderCTP::OnRtnOrder(CThostFtdcOrderField *pOrder)
            {
                ORDER_TRACE(to_string(*pOrder));
                order_mapper_->update_sys_id(pOrder->FrontID, pOrder->SessionID, pOrder->OrderRef, pOrder->ExchangeID, pOrder->OrderSysID);
                auto order_info = order_mapper_->get_order_info_by_order_ref(pOrder->FrontID, pOrder->SessionID, pOrder->OrderRef);
                if (order_info.internal_order_id == 0)
                {
                    ORDER_ERROR(fmt::format("can't find FrontID {} SessionID {} OrderRef {}", pOrder->FrontID, pOrder->SessionID, pOrder->OrderRef));
                }
                else
                {
                    auto writer = get_writer(order_info.source);
                    msg::data::Order &order = writer->open_data<msg::data::Order>(0, msg::type::Order);
                    from_ctp(*pOrder, order);
                    order.order_id = order_info.internal_order_id;
                    order.parent_id = order_info.parent_id;
                    order.insert_time = order_info.insert_time;
                    order.update_time = kungfu::yijinjing::time::now_in_nano();
                    writer->close_data();
                }
            }

            void TraderCTP::OnRtnTrade(CThostFtdcTradeField *pTrade)
            {
                TRADE_TRACE(to_string(*pTrade));
                auto order_info = order_mapper_->get_order_info_by_sys_id(pTrade->ExchangeID, pTrade->OrderSysID);
                if (order_info.internal_order_id == 0)
                {
                    TRADE_ERROR(fmt::format("can't find ExchangeID {} and OrderSysID {}", pTrade->ExchangeID, pTrade->OrderSysID));
                }
                else
                {
                    auto writer = get_writer(order_info.source);
                    msg::data::Trade &trade = writer->open_data<msg::data::Trade>(0, msg::type::Trade);
                    from_ctp(*pTrade, trade);
                    uint64_t trade_id = writer->current_frame_uid();
                    trade.trade_id = trade_id;
                    trade.order_id = order_info.internal_order_id;
                    trade.parent_order_id = order_info.parent_id;
                    writer->close_data();

                    trade_mapper_->save(trade_id, kungfu::yijinjing::time::now_in_nano(), pTrade->TradingDay, pTrade->ExchangeID, pTrade->TradeID);

                }
            }

            void TraderCTP::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount,
                                                   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    ACCOUNT_ERROR(fmt::format("(error_id) {} (error_msg) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg)));
                }
                else
                {
                    ACCOUNT_TRACE(to_string(*pTradingAccount));
                    auto writer = get_writer(0);
                    msg::data::Asset &account = writer->open_data<msg::data::Asset>(0, msg::type::Asset);
                    strcpy(account.account_id, get_account_id().c_str());
                    from_ctp(*pTradingAccount, account);
                    account.update_time = kungfu::yijinjing::time::now_in_nano();
                    writer->close_data();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    req_position_detail();
                }
            }

            void TraderCTP::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition,
                                                     CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
            {}

            void TraderCTP::OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail,
                                                           CThostFtdcRspInfoField *pRspInfo, int nRequestID,
                                                           bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    POSITION_DETAIL_ERROR(fmt::format("(error_id) {} (error_msg) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg)));
                } else
                {
                    auto writer = get_writer(0);
                    if (pInvestorPositionDetail != nullptr)
                    {
                        POSITION_DETAIL_TRACE(to_string(*pInvestorPositionDetail));
                        msg::data::PositionDetail &pos_detail = writer->open_data<msg::data::PositionDetail>(0, msg::type::PositionDetail);
                        from_ctp(*pInvestorPositionDetail, pos_detail);
                        pos_detail.update_time = kungfu::yijinjing::time::now_in_nano();
                        CtpTrade trade_info = trade_mapper_->get(pInvestorPositionDetail->OpenDate, pInvestorPositionDetail->ExchangeID, pInvestorPositionDetail->TradeID);
                        if (trade_info.trade_id != 0)
                        {
                            pos_detail.trade_id = trade_info.trade_id;
                            pos_detail.trade_time = trade_info.trade_time;
                        }
                        else
                        {
                            pos_detail.trade_id = writer->current_frame_uid();
                        }
                        writer->close_data();
                    }
                }
                if (bIsLast)
                {
                    POSITION_DETAIL_TRACE(fmt::format("PositionDetailEnd, RequestID {}", nRequestID));
                    get_writer(0)->mark(0, msg::type::PositionDetailEnd);
                }
            }

            void TraderCTP::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo,
                                               int nRequestID, bool bIsLast)
            {
                if (pRspInfo != nullptr && pRspInfo->ErrorID != 0)
                {
                    INSTRUMENT_ERROR(fmt::format("(error_id) {} (error_msg) {}", pRspInfo->ErrorID, gbk2utf8(pRspInfo->ErrorMsg)));
                } else
                {
                    INSTRUMENT_TRACE(kungfu::wingchun::ctp::to_string(*pInstrument));
                    auto writer = get_writer(0);
                    if (pInstrument->ProductClass == THOST_FTDC_PC_Futures)
                    {
                        msg::data::Instrument &instrument = writer->open_data<msg::data::Instrument>(0, msg::type::Instrument);
                        from_ctp(*pInstrument, instrument);
                        writer->close_data();
                    }
                    if (bIsLast)
                    {
                        writer->mark(0, msg::type::InstrumentEnd);
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        req_account();
                    }
                }
            }
        }
    }
}

