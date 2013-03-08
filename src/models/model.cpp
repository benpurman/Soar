#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <iterator>
#include <limits>
#include "serialize.h"
#include "model.h"

using namespace std;


void slice(const rvec &src, rvec &tgt, const vector<int> &srcinds, const vector<int> &tgtinds) {
	if (srcinds.empty() && tgtinds.empty()) {
		int n = max(src.size(), tgt.size());
		tgt.head(n) = src.head(n);
	} else if (srcinds.empty()) {
		for (int i = 0; i < tgtinds.size(); ++i) {
			tgt(tgtinds[i]) = src(i);
		}
	} else {
		for (int i = 0; i < srcinds.size(); ++i) {
			tgt(i) = src(srcinds[i]);
		}
	}
}

bool find_prop_inds(const scene_sig &sig, const multi_model::prop_vec &pv, vector<int> &obj_inds, vector<int> &prop_inds) {
	int oind, pind;
	for (int i = 0; i < pv.size(); ++i) {
		const string &obj = pv[i].first;
		const string &prop = pv[i].second;
		if (!sig.get_dim(pv[i].first, pv[i].second, oind, pind)) {
			return false;
		}
		if (obj_inds.empty() || obj_inds.back() != oind) {
			obj_inds.push_back(oind);
		}
		prop_inds.push_back(pind);
	}
	return true;
}

bool error_stats(const vector<double> &errors, ostream &os) {
	double total = 0.0, std = 0.0, q1, q2, q3;
	int num_nans = 0;
	vector<double> ds;
	
	for (int i = 0, iend = errors.size(); i < iend; ++i) {
		if (isnan(errors[i])) {
			++num_nans;
		} else {
			ds.push_back(errors[i]);
			total += errors[i];
		}
	}
	double last = ds.back();
	if (ds.empty()) {
		os << "no predictions" << endl;
		return false;
	}
	double mean = total / ds.size();
	for (int i = 0; i < ds.size(); ++i) {
		std += pow(ds[i] - mean, 2);
	}
	std = sqrt(std / ds.size());
	sort(ds.begin(), ds.end());
	
	sort(ds.begin(), ds.end());
	q1 = ds[ds.size() / 4];
	q2 = ds[ds.size() / 2];
	q3 = ds[(ds.size() / 4) * 3];
	
	table_printer t;
	t.add_row() << "mean" << "std" << "min" << "q1" << "q2" << "q3" << "max" << "last" << "failed";
	t.add_row() << mean << std << ds.front() << q1 << q2 << q3 << ds.back() << last << num_nans;
	t.print(os);

	return true;
}

model::model(const std::string &name, const std::string &type, bool learning) 
: name(name), type(type), learning(learning)
{}

void model::learn(int target, const scene_sig &sig, const relation_table &rels, const rvec &x, const rvec &y) {
	if (learning) {
		train_data.add(target, sig, rels, x, y);
		update();
	}
}

/*
 The default test behavior is just to return the prediction. The EM
 model will also record mode prediction information.
*/
void model::test(int target, const scene_sig &sig, const relation_table &rels, const rvec &x, rvec &y) {
	predict(target, sig, rels, x, y);
}

bool model::cli_inspect(int first_arg, const vector<string> &args, ostream &os) {
	if (first_arg < args.size()) {
		if (args[first_arg] == "save") {
			string path;
			if (first_arg + 1 >= args.size()) {
				path = name + ".model";
			} else {
				path = args[first_arg + 1];
			}
			ofstream f(path.c_str());
			if (!f.is_open()) {
				os << "cannot open file " << path << " for writing" << endl;
				return false;
			}
			serialize(f);
			f.close();
			os << "saved to " << path << endl;
			return true;
		} else if (args[first_arg] == "load") {
			string path;
			if (first_arg + 1 >= args.size()) {
				path = name + ".model";
			} else {
				path = args[first_arg + 1];
			}
			ifstream f(path.c_str());
			if (!f.is_open()) {
				os << "cannot open file " << path << " for reading" << endl;
				return false;
			}
			unserialize(f);
			f.close();
			os << "loaded from " << path << endl;
			return true;
		} else if (args[first_arg] == "relations") {
			return train_data.cli_inspect_relations(first_arg + 1, args, os);
		}
	}
	return cli_inspect_sub(first_arg, args, os);
}
bool model_train_data::cli_inspect_relations(int i, const vector<string> &args, ostream &os) const {
	const relation_table *rels;
	if (i < args.size() && args[i] == "close") {
		rels = &context_rels;
		++i;
	} else {
		rels = &all_rels;
	}
	
	if (i >= args.size()) {
		os << *rels << endl;
		return true;
	}
	const relation *r = map_getp(*rels, args[i]);
	if (!r) {
		os << "no such relation" << endl;
		return false;
	}
	if (i + 1 >= args.size()) {
		os << *r << endl;
		return true;
	}

	relation matches(*r);

	tuple t(1);
	int j, k;
	for (j = i + 1, k = 0; j < args.size() && k < matches.arity(); ++j, ++k) {
		if (args[j] != "*") {
			if (!parse_int(args[j], t[0])) {
				os << "invalid pattern" << endl;
				return false;
			}
			matches.filter(k, t, false);
		}
	}

	os << matches << endl;
	return true;
}

void model::serialize(ostream &os) const {
	serializer sr(os);
	sr << name << type << learning << '\n';
	sr << train_data << '\n';
	serialize_sub(os);
}

void model::unserialize(istream &is) {
	unserializer(is) >> name >> type >> learning >> train_data;
	unserialize_sub(is);
}

multi_model::multi_model(map<string, model*> *model_db) : model_db(model_db) {}

multi_model::~multi_model() {
	std::list<model_config*>::iterator i;
	for (i = active_models.begin(); i != active_models.end(); ++i) {
		delete *i;
	}
}

bool multi_model::predict(const scene_sig &sig, const relation_table &rels, const rvec &x, rvec &y) {
	return predict_or_test(false, sig, rels, x, y);
}

/*
 When testing, the expectation is that y initially contains the
 reference values.
*/
bool multi_model::predict_or_test(bool test, const scene_sig &sig, const relation_table &rels, const rvec &x, rvec &y) {
	rvec yorig = y;
	std::list<model_config*>::const_iterator i;
	for (i = active_models.begin(); i != active_models.end(); ++i) {
		model_config *cfg = *i;
		assert(cfg->allx); // don't know what to do with the signature when we have to slice
		
		rvec yp(cfg->yprops.size());
		yp.setConstant(NAN);
		vector<int> yinds, yobjs;
		
		find_prop_inds(sig, cfg->yprops, yobjs, yinds);
		/*
		 I'm going to start making the assumption that all
		 models only predict the properties of a single
		 object. Clean this part up later.
		*/
		assert(yobjs.size() == 1);
		if (test) {
			slice(yorig, yp, yinds, vector<int>());
			cfg->mdl->test(yobjs[0], sig, rels, x, yp);
		} else {
			cfg->mdl->predict(yobjs[0], sig, rels, x, yp);
		}
		slice(yp, y, vector<int>(), yinds);
	}
	return true;
}

void multi_model::learn(const scene_sig &sig, const relation_table &rels, const rvec &x, const rvec &y) {
	std::list<model_config*>::iterator i;
	int j;
	for (i = active_models.begin(); i != active_models.end(); ++i) {
		model_config *cfg = *i;
		assert(cfg->allx); // don't know what to do with the signature when we have to slice
		
		rvec yp;
		vector<int> yinds, yobjs;
		
		find_prop_inds(sig, cfg->yprops, yobjs, yinds);
		assert(yobjs.size() == 1);
		yp.resize(yinds.size());
		slice(y, yp, yinds, vector<int>());
		cfg->mdl->learn(yobjs[0], sig, rels, x, yp);
	}
}

void multi_model::test(const scene_sig &sig, const relation_table &rels, const rvec &x, const rvec &y) {
	test_info &t = grow_vec(tests);
	t.sig = sig;
	t.x = x;
	t.y = y;
	t.pred = y;
	predict_or_test(true, sig, rels, x, t.pred);
	t.error = (t.y - t.pred).array().abs();
}

string multi_model::assign_model
( const string &name, 
  const prop_vec &inputs, bool all_inputs,
  const prop_vec &outputs)
{
	model *m;
	model_config *cfg;
	if (!map_get(*model_db, name, m)) {
		return "no model";
	}
	
	cfg = new model_config();
	cfg->name = name;
	cfg->mdl = m;
	
	cfg->allx = all_inputs;
	if (!all_inputs) {
		if (m->get_input_size() >= 0 && m->get_input_size() != inputs.size()) {
			return "size mismatch";
		}
		cfg->xprops = inputs;
	}
	
	if (m->get_output_size() >= 0 && m->get_output_size() != outputs.size()) {
		return "size mismatch";
	}
	cfg->yprops = outputs;
	
	active_models.push_back(cfg);
	return "";
}

void multi_model::unassign_model(const string &name) {
	std::list<model_config*>::iterator i;
	for (i = active_models.begin(); i != active_models.end(); ++i) {
		if ((**i).name == name) {
			active_models.erase(i);
			return;
		}
	}
}

bool multi_model::report_error(int i, const vector<string> &args, ostream &os) const {
	int start = 0, end = tests.size() - 1;
	vector<double> y, preds, errors;
	vector<string> obj_prop;
	enum { STATS, LIST, HISTO, DUMP } mode = STATS;
	
	if (tests.empty()) {
		os << "no test error data" << endl;
		return false;
	}
	
	if (i >= args.size()) {
		os << "specify object:property" << endl;
		return false;
	}

	split(args[i++], ":", obj_prop);
	if (obj_prop.size() != 2) {
		os << "invalid object:property" << endl;
		return false;
	}
	
	if (i < args.size() && args[i] == "list") {
		mode = LIST;
		++i;
	} else if (i < args.size() && args[i] == "histogram") {
		mode = HISTO;
		++i;
	} else if (i < args.size() && args[i] == "dump") {
		mode = DUMP;
		++i;
	}
	
	if (i < args.size()) {
		if (!parse_int(args[i], start)) {
			os << "require integer start time" << endl;
			return false;
		}
		if (start < 0 || start >= tests.size()) {
			os << "start time must be in [0, " << tests.size() - 1 << "]" << endl;
			return false;
		}
		if (++i < args.size()) {
			if (!parse_int(args[i], end)) {
				os << "require integer end time" << endl;
				return false;
			}
			if (end <= start || end >= tests.size()) {
				os << "end time must be in [start + 1, " << tests.size() - 1 << "]" << endl;
				return false;
			}
		}
	}
	
	for (int i = start; i <= end; ++i) {
		int obj_index, prop_index;
		if (tests[i].sig.get_dim(obj_prop[0], obj_prop[1], obj_index, prop_index)) {
			y.push_back(tests[i].y(prop_index));
			preds.push_back(tests[i].pred(prop_index));
			errors.push_back(tests[i].error(prop_index));
		} else {
			y.push_back(NAN);
			preds.push_back(NAN);
			errors.push_back(NAN);
		}
	}
	
	switch (mode) {
	case STATS:
		return error_stats(errors, os);
	case LIST:
		{
			table_printer t;
			t.add_row() << "num" << "real" << "pred" << "error" << "null" << "norm";
			for (int i = 0, iend = y.size(); i < iend; ++i) {
				t.add_row() << i << y[i] << preds[i] << errors[i];
				if (i > 0) {
					double null_error = fabs(y[i-1] - y[i]);
					t << null_error << errors[i] / null_error;
				} else {
					t << "NA" << "NA";
				}
			}
			t.print(os);
		}
		return true;
	case HISTO:
		histogram(errors, 20, os);
		os << endl;
		return true;
	case DUMP:
		{
			table_printer t;
			t.add_row() << "real" << "pred";
			for (int i = 0, iend = y.size(); i < iend; ++i) {
				t.add_row() << y[i] << preds[i];
			}
			t.print(os);
		}
		return true;
	}
	return false;
}

void multi_model::report_model_config(model_config* c, ostream &os) const {
	const char *indent = "  ";
	os << c->name << endl;
	if (c->allx) {
		os << indent << "xdims: all" << endl;
	} else {
		os << indent << "xdims: ";
		for (int i = 0; i < c->xprops.size(); ++i) {
			os << c->xprops[i].first << ":" << c->xprops[i].second << " ";
		}
		os << endl;
	}
	os << indent << "ydims: ";
	for (int i = 0; i < c->yprops.size(); ++i) {
		os << c->yprops[i].first << ":" << c->yprops[i].second << " ";
	}
	os << endl;
}

bool multi_model::cli_inspect(int i, const vector<string> &args, ostream &os) const {
	if (i >= args.size()) {
		os << "available subqueries are: assignment error" << endl;
		return false;
	}
	if (args[i] == "assignment") {
		std::list<model_config*>::const_iterator j;
		for (j = active_models.begin(); j != active_models.end(); ++j) {
			report_model_config(*j, os);
		}
		return true;
	} else if (args[i] == "error") {
		return report_error(++i, args, os);
	}
	os << "no such query" << endl;
	return false;
}

model_train_data::model_train_data() { }

model_train_data::~model_train_data() {
	clear_and_dealloc(sigs);
	clear_and_dealloc(insts);
}

void model_train_data::add(int target, const scene_sig &sig, const relation_table &r, const rvec &x, const rvec &y) {
	model_train_inst *inst = new model_train_inst;
	for (int i = 0, iend = sigs.size(); i < iend; ++i) {
		if (*sigs[i] == sig) {
			inst->sig_index = i;
			inst->sig = sigs[i];
			break;
		}
	}
	
	if (inst->sig == NULL) {
		scene_sig *newsig = new scene_sig;
		*newsig = sig;
		sigs.push_back(newsig);
		inst->sig_index = sigs.size() - 1;
		inst->sig = newsig;
	}
	
	inst->x = x;
	inst->y = y;
	inst->target = target;
	insts.push_back(inst);
	
	relation_table c;
	extend_relations(all_rels, r, insts.size() - 1);
	::get_context_rels(sig[target].id, r, c);
	extend_relations(context_rels, c, insts.size() - 1);
}

void model_train_data::serialize(ostream &os) const {
	serializer sr(os);
	
	sr << "MODEL_TRAIN_DATA" << sigs.size() << insts.size() << '\n';
	for (int i = 0, iend = sigs.size(); i < iend; ++i) {
		sigs[i]->serialize(os);
	}
	sr << '\n';
	
	for (int i = 0, iend = insts.size(); i < iend; ++i) {
		const model_train_inst *inst = insts[i];
		sr << inst->sig_index << inst->target << static_cast<int>(inst->x.size()) << static_cast<int>(inst->y.size());
		for (int i = 0, iend = inst->x.size(); i < iend; ++i) {
			sr << inst->x(i);
		}
		for (int i = 0, iend = inst->y.size(); i < iend; ++i) {
			sr << inst->y(i);
		}
		sr << '\n';
	}
	sr << '\n';
	
	sr << all_rels << context_rels;
}

void model_train_data::unserialize(istream &is) {
	string line, label;
	int nsigs, ninsts;

	unserializer unsr(is);
	
	unsr >> label >> nsigs >> ninsts;
	assert(label == "MODEL_TRAIN_DATA");
	
	for (int i = 0; i < nsigs; ++i) {
		scene_sig *s = new scene_sig;
		s->unserialize(is);
		sigs.push_back(s);
	}
	
	for (int i = 0; i < ninsts; ++i) {
		model_train_inst *inst = new model_train_inst;
		int xsz, ysz;
		unsr >> inst->sig_index >> inst->target >> xsz >> ysz;
		inst->sig = sigs[inst->sig_index];
		inst->x.resize(xsz);
		inst->y.resize(ysz);
		for (int i = 0; i < xsz; ++i) {
			unsr >> inst->x(i);
		}
		for (int i = 0; i < ysz; ++i) {
			unsr >> inst->y(i);
		}
		insts.push_back(inst);
	}
	
	unsr >> all_rels >> context_rels;
}
