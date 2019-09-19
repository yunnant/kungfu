import os
import sys
import importlib
import pywingchun
import pyyjj
from kungfu.data.sqlite.data_proxy import LedgerDB
from kungfu.wingchun.finance.book import *
from kungfu.wingchun.constants import *
import kungfu.yijinjing.time as kft

class Strategy(pywingchun.Strategy):
    def __init__(self, ctx):
        pywingchun.Strategy.__init__(self)
        ctx.log = ctx.logger
        ctx.strftime = kft.strftime
        ctx.strptime = kft.strptime
        ctx.inst_infos = {}
        self.ctx = ctx
        self.ctx.book = None
        self.__init_strategy(ctx.path)

    def __init_ledger(self):
        ledger_location = pyyjj.location(pyyjj.get_mode_by_name(self.ctx.mode), pyyjj.category.SYSTEM, 'service', 'ledger', self.ctx.locator)
        self.ctx.ledger_db = LedgerDB(ledger_location, "ledger")
        self.ctx.inst_infos = {inst["instrument_id"]: inst for inst in self.ctx.ledger_db.all_instrument_infos()}
        book_tags = AccountBookTags(ledger_category=LedgerCategory.Portfolio, client_id=self.ctx.name)
        self.ctx.book = self.ctx.ledger_db.load(ctx=self.ctx, book_tags=book_tags)
        if self.ctx.book is None:
            self.ctx.book = AccountBook(self.ctx,tags=book_tags, avail=1e7)

    def __get_inst_info(self, instrument_id):
        if instrument_id not in self.ctx.inst_infos:
            self.ctx.inst_infos[instrument_id] = self.ctx.ledger_db.get_instrument_info(instrument_id)
        return self.ctx.inst_infos[instrument_id]

    def __init_strategy(self, path):
        strategy_dir = os.path.dirname(path)
        name_no_ext = os.path.split(os.path.basename(path))
        sys.path.append(os.path.relpath(strategy_dir))
        impl = importlib.import_module(os.path.splitext(name_no_ext[1])[0])
        self._pre_start = getattr(impl, 'pre_start', lambda ctx: None)
        self._post_start = getattr(impl, 'post_start', lambda ctx: None)
        self._pre_stop = getattr(impl, 'pre_stop', lambda ctx: None)
        self._post_stop = getattr(impl, 'post_stop', lambda ctx: None)
        self._on_trading_day = getattr(impl, "on_trading_day", lambda ctx, trading_day: None)
        self._on_quote = getattr(impl, 'on_quote', lambda ctx, quote: None)
        self._on_entrust = getattr(impl, 'on_entrust', lambda ctx, entrust: None)
        self._on_transaction = getattr(impl, "on_transaction", lambda ctx, transaction: None)
        self._on_order = getattr(impl, 'on_order', lambda ctx, order: None)
        self._on_trade = getattr(impl, 'on_trade', lambda ctx, trade: None)

    def __add_timer(self, nanotime, callback):
        def wrap_callback(event):
            callback(self.ctx, event)
        self.wc_context.add_timer(nanotime, wrap_callback)

    def __add_time_interval(self, duration, callback):
        def wrap_callback(event):
            callback(self.ctx, event)
        self.wc_context.add_time_interval(duration, wrap_callback)

    def pre_start(self, wc_context):
        self.ctx.logger.info("pre start")
        self.wc_context = wc_context
        self.ctx.now = wc_context.now
        self.ctx.add_timer = self.__add_timer
        self.ctx.add_time_interval = self.__add_time_interval
        self.ctx.get_inst_info = self.__get_inst_info
        self.ctx.subscribe = wc_context.subscribe
        self.ctx.add_account = wc_context.add_account
        self.ctx.list_accounts = wc_context.list_accounts
        self.ctx.get_account_cash_limit = wc_context.get_account_cash_limit
        self.ctx.insert_order = wc_context.insert_order
        self.ctx.cancel_order = wc_context.cancel_order
        self.__init_ledger()
        self._pre_start(self.ctx)
        self.ctx.log.info('strategy prepare to run')

    def post_start(self, wc_context):
        self._post_start(self.ctx)
        self.ctx.log.info('strategy ready to run')

    def pre_stop(self, wc_context):
        self._pre_stop(self.ctx)

    def post_stop(self, wc_context):
        self._post_stop(self.ctx)

    def on_quote(self, wc_context, quote):
        self._on_quote(self.ctx, quote)
        self.ctx.book.apply_quote(quote)

    def on_entrust(self, wc_context, entrust):
        self._on_entrust(self.ctx, entrust)

    def on_transaction(self, wc_context, transaction):
        self._on_transaction(self.ctx, transaction)

    def on_order(self, wc_context, order):
        self._on_order(self.ctx, order)

    def on_trade(self, wc_context, trade):
        self._on_trade(self.ctx, trade)
        self.ctx.book.apply_trade(trade)

    def on_trading_day(self, wc_context, daytime):
        trading_day = kft.to_datetime(daytime)
        self.ctx.logger.info("on trading day {}".format(trading_day))
        if self.ctx.book:
            self.ctx.book.apply_trading_day(trading_day)
        self.ctx.trading_day = trading_day
        self.ctx.logger.info("assign trading day {} for ctx".format(trading_day))
        self._on_trading_day(self.ctx, daytime)