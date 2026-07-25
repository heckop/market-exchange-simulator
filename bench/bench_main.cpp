#include <iostream>
#include <iomanip>
#include <fstream>
#include "Histogram.hpp"
#include "Timer.hpp"
#include "OrderGenerator.hpp"
import OrderBookEngine;


Trading::Order to_add(const Action& a) {
	return {
		a.id,
		a.price,
		a.quantity,
		a.quantity,
		static_cast<Trading::Side>(a.side),
		static_cast<Trading::OrderType>(a.type)
	};
}

Trading::OrderModify to_modify(const Action& a) {
	return {
		a.id,
		a.quantity,
		a.price,
		static_cast<Trading::Side>(a.side)
	};
}

int main() {
	Scenario scn;
	scn.num_orders = 5000000;
	OrderGenerator gen(scn);
	auto actions = gen.generate();
	Histogram h_add, h_modify, h_cancel;
	Trading::OrderBook book, clean_book;
	for (const auto& a : actions) {
		switch (a.kind) {
			case ActionKind::Add : {
				std::int64_t t0 = bench::now_ns();
				book.add_order(to_add(a));
				std::int64_t t1 = bench::now_ns();
				h_add.record(t1-t0);
				break;
			}
			case ActionKind::Cancel: {
				std::int64_t t0 = bench::now_ns();
				book.cancel_order(a.id);
				std::int64_t t1 = bench::now_ns();
				h_cancel.record(t1-t0);
				break;
			}
			case ActionKind::Modify: {
				std::int64_t t0 = bench::now_ns();
				book.modify_order(to_modify(a));
				std::int64_t t1 = bench::now_ns();
				h_modify.record(t1-t0);
				break;
			}
		}
	}

	std::int64_t start = bench::now_ns();
	for (const auto& a: actions) {
		switch (a.kind) {
			case ActionKind::Add : clean_book.add_order(to_add(a)); break;
			case ActionKind::Cancel : clean_book.cancel_order(a.id); break;
			case ActionKind::Modify : clean_book.modify_order(to_modify(a)); break;
		}
	}
	std::int64_t end = bench::now_ns();
	double time_sec = (end-start)/1e9;
	double order_per_sec = actions.size()/time_sec;
	auto report = [](const char* name, Histogram &h) {
		std::cout << name<< " count=" << h.count()
			<< "  p50=" << h.percentile(0.5)
			<< "  p99=" << h.percentile(0.99)
			<< "  p99.9=" << h.percentile(0.999)
			<< "  max=" << h.max() << " ns\n";
	};
	report("add_order", h_add);
	report("cancel", h_cancel);
	report("modify", h_modify);
	std::ofstream csv("./report.csv");
	std::cout<< std::fixed<<std::setprecision(6)<<"throughput per sec "<< order_per_sec<<'\n';
	csv << std::setw(6)<<"operation,count,p50,p99,p99_9,max\n";
	csv << std::setw(6) << "add_order," << h_add.count() <<','<< h_add.percentile(0.5) <<','<< h_add.percentile(0.99) <<','<< h_add.percentile(0.999) <<','<< h_add.max()<<'\n';
	csv << std::setw(6) << "cancel_order," << h_cancel.count() <<','<< h_cancel.percentile(0.5) <<','<< h_cancel.percentile(0.99) <<','<< h_cancel.percentile(0.999) <<','<< h_cancel.max()<<'\n';
	csv << std::setw(6) << "modify_order," << h_modify.count() <<','<< h_modify.percentile(0.5) <<','<< h_modify.percentile(0.99) <<','<< h_modify.percentile(0.999) <<','<< h_modify.max()<<'\n';
}