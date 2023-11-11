#include "Objects/AlgExpression.h"
#include "Objects/Language.h"
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>

AlgExpression::AlgExpression() {
	type = AlgExpression::eps;
}

AlgExpression::AlgExpression(shared_ptr<Language> language, Type type, const Lexeme& value,
							 const set<alphabet_symbol>& alphabet)
	: BaseObject(move(language)), type(type), value(value), alphabet(alphabet) {}

AlgExpression::AlgExpression(set<alphabet_symbol> alphabet) : BaseObject(move(alphabet)) {}

AlgExpression::Lexeme::Lexeme(Type type, const alphabet_symbol& symbol, int number)
	: type(type), symbol(symbol), number(number) {}

void AlgExpression::clear() {
	if (term_l != nullptr) {
		delete term_l;
		term_l = nullptr;
	}
	if (term_r != nullptr) {
		delete term_r;
		term_r = nullptr;
	}
}

AlgExpression::~AlgExpression() {
	clear();
}

AlgExpression::AlgExpression(const AlgExpression& other) : AlgExpression() {
	alphabet = other.alphabet;
	type = other.type;
	value = other.value;
	language = other.language;
	if (other.term_l != nullptr)
		term_l = other.term_l->make_copy();
	if (other.term_r != nullptr)
		term_r = other.term_r->make_copy();
}

AlgExpression& AlgExpression::operator=(const AlgExpression& other) {
	if (this != &other) {
		clear();
		copy(&other);
	}
	return *this;
}

void AlgExpression::set_language(const set<alphabet_symbol>& _alphabet) {
	alphabet = _alphabet;
	language = make_shared<Language>(alphabet);
}

void AlgExpression::generate_alphabet(set<alphabet_symbol>& _alphabet) {
	if (term_l != nullptr) {
		term_l->generate_alphabet(alphabet);
	}
	if (term_r != nullptr) {
		term_r->generate_alphabet(alphabet);
	}
	for (const auto& symb : alphabet) {
		_alphabet.insert(symb);
	}
}

void AlgExpression::make_language() {
	generate_alphabet(alphabet);
	language = make_shared<Language>(alphabet);
}

bool AlgExpression::is_terminal_type(Type t) {
	return t == Type::symb || t == Type::cellWriter || t == Type::ref;
}

string AlgExpression::to_txt() const {
	string str1, str2;
	if (term_l) {
		str1 = term_l->to_txt();
	}
	if (term_r) {
		str2 = term_r->to_txt();
	}
	string symb;
	switch (type) {
	case Type::conc:
		if (term_l && term_l->type == Type::alt) {
			str1 = "(" + str1 + ")";
		}
		if (term_r && term_r->type == Type::alt) {
			str2 = "(" + str2 + ")";
		}
		break;
	case Type::symb:
		symb = value.symbol;
		break;
	case Type::eps:
		symb = "";
		break;
	case Type::alt:
		symb = '|';
		break;
	case Type::star:
		symb = '*';
		if (!is_terminal_type(term_l->type))
			// ставим скобки при итерации, если символов > 1
			str1 = "(" + str1 + ")";
		break;
	case Type::negative:
		symb = '^';
		return symb + str1;
	default:
		break;
	}

	return str1 + symb + str2;
}

// для дебага
void AlgExpression::print_subtree(AlgExpression* expr, int level) const {
	if (expr) {
		print_subtree(expr->term_l, level + 1);
		for (int i = 0; i < level; i++)
			cout << "   ";
		alphabet_symbol r_v;
		if (expr->value.symbol != "")
			r_v = expr->value.symbol;
		else
			r_v = to_string(expr->type);
		cout << r_v << endl;
		print_subtree(expr->term_r, level + 1);
	}
}

void AlgExpression::print_tree() const {
	print_subtree(term_l, 1);
	for (int i = 0; i < 0; i++)
		cout << "   ";
	alphabet_symbol r_v;
	if (value.symbol != "")
		r_v = value.symbol;
	else
		r_v = to_string(type);
	cout << r_v << endl;
	print_subtree(term_r, 1);
}

string AlgExpression::type_to_str() const {
	if (value.symbol != "")
		return value.symbol;
	switch (type) {
	case Type::eps:
		return "ε";
	case Type::alt:
		return "|";
	case Type::conc:
		return ".";
	case Type::star:
		return "*";
	case Type::symb:
		return "symb";
	case Type::negative:
		return "^";
	default:
		break;
	}
	return {};
}

string AlgExpression::print_subdot(AlgExpression* expr, const string& parent_dot_node,
								   int& id) const {
	string dot;
	if (expr) {
		string dot_node = "node" + to_string(id++);

		alphabet_symbol r_v;
		r_v = expr->type_to_str();

		dot += dot_node + " [label=\"" + string(r_v) + "\"];\n";

		if (!parent_dot_node.empty()) {
			dot += parent_dot_node + " -- " + dot_node + ";\n";
		}

		dot += print_subdot(expr->term_l, dot_node, id);
		dot += print_subdot(expr->term_r, dot_node, id);
	}
	return dot;
}

void AlgExpression::print_dot() const {
	int id = 0;

	string dot;
	dot += "graph {\n";

	alphabet_symbol r_v;
	r_v = type_to_str();

	string root_dot_node = "node" + to_string(id++);
	dot += root_dot_node + " [label=\"" + string(r_v) + "\"];\n";

	dot += print_subdot(term_l, root_dot_node, id);
	dot += print_subdot(term_r, root_dot_node, id);

	dot += "}\n";
	cout << dot << endl;
}

bool read_number(const string& str, size_t& pos, int& res) {
	if (pos >= str.size() || !isdigit(str[pos])) {
		return false;
	}

	res = 0;
	while (pos < str.size() && isdigit(str[pos])) {
		res = res * 10 + (str[pos] - '0');
		pos++;
	}

	pos--;
	return true;
}

vector<AlgExpression::Lexeme> AlgExpression::parse_string(string str, bool allow_ref,
														  bool allow_negation) {
	vector<AlgExpression::Lexeme> lexemes;
	stack<char> brackets_checker;
	bool brackets_are_empty = true;
	stack<size_t> memory_opening_indexes;

	bool regex_is_eps = true;
	auto is_symbol = [](char c) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'; };

	for (size_t index = 0; index < str.size(); index++) {
		char c = str[index];
		Lexeme lexeme;
		switch (c) {
		case '^':
			if (!allow_negation)
				return {Lexeme::Type::error};

			lexeme.type = Lexeme::Type::negative;
			break;
		case '(':
			lexeme.type = Lexeme::Type::parL;
			brackets_checker.push('(');
			brackets_are_empty = true;
			break;
		case ')':
			lexeme.type = Lexeme::Type::parR;
			if (brackets_are_empty || brackets_checker.empty() || brackets_checker.top() != '(')
				return {Lexeme::Type::error};

			brackets_checker.pop();
			break;
		case '[':
			if (!allow_ref)
				return {Lexeme::Type::error};

			lexeme.type = Lexeme::Type::squareBrL;
			brackets_checker.push('[');
			brackets_are_empty = true;
			break;
		case ']':
			if (brackets_are_empty || brackets_checker.empty() || brackets_checker.top() != '[')
				return {Lexeme::Type::error};
			brackets_checker.pop();

			index++;
			if (index >= str.size() && str[index] != ':')
				return {Lexeme::Type::error};

			if (!read_number(str, ++index, lexeme.number))
				return {Lexeme::Type::error};

			lexeme.type = Lexeme::Type::squareBrR;
			lexemes[memory_opening_indexes.top()].number = lexeme.number;
			memory_opening_indexes.pop();
			break;
		case '&':
			if (!allow_ref)
				return {Lexeme::Type::error};

			if (!read_number(str, ++index, lexeme.number))
				return {Lexeme::Type::error};

			lexeme.type = Lexeme::Type::ref;
			regex_is_eps = false;
			brackets_are_empty = false;
			break;
		case '|':
			if (index != 0 && lexemes.back().type == Lexeme::Type::negative)
				return {Lexeme::Type::error};

			lexeme.type = Lexeme::Type::alt;
			break;
		case '*':
			if (index == 0 || (index != 0 && (lexemes.back().type == Lexeme::Type::star ||
											  lexemes.back().type == Lexeme::Type::alt ||
											  lexemes.back().type == Lexeme::Type::negative)))
				return {Lexeme::Type::error};

			lexeme.type = Lexeme::Type::star;
			break;
		default:
			if (is_symbol(c)) {
				lexeme.type = Lexeme::Type::symb;
				lexeme.symbol = c;
				for (size_t j = index + 1; j < str.size(); j++) {
					bool lin = false;
					bool annote = false;
					if (str[j] == alphabet_symbol::linearize_marker)
						lin = true;

					if (str[j] == alphabet_symbol::annote_marker)
						annote = true;

					if (!lin && !annote)
						break;

					int number;
					if (!read_number(str, ++j, number))
						return {Lexeme::Type::error};
					index = j;

					if (lin)
						lexeme.symbol.linearize(number);

					if (annote)
						lexeme.symbol.annote(number);
				}

				regex_is_eps = false;
				brackets_are_empty = false;
			} else
				return {Lexeme::Type::error};
			break;
		}

		if (!lexemes.empty() &&
			(
				// AlgExpression::Lexeme left
				lexemes.back().type == Lexeme::Type::symb ||
				lexemes.back().type == Lexeme::Type::star ||
				lexemes.back().type == Lexeme::Type::parR ||
				lexemes.back().type == Lexeme::Type::squareBrR ||
				lexemes.back().type == Lexeme::Type::ref) &&
			(
				// AlgExpression::Lexeme right
				lexeme.type == Lexeme::Type::symb || lexeme.type == Lexeme::Type::parL ||
				lexeme.type == Lexeme::Type::squareBrL || lexeme.type == Lexeme::Type::ref ||
				lexeme.type == Lexeme::Type::negative)) {
			// We place . between
			lexemes.emplace_back(Lexeme::Type::conc);
		}

		if (!lexemes.empty() &&
			((lexeme.type == Lexeme::Type::alt &&
			  (lexemes.back().type == Lexeme::Type::parL ||
			   lexemes.back().type == Lexeme::Type::squareBrL)) ||
			 ((lexemes.back().type == Lexeme::Type::alt || lexemes.back().type == Lexeme::Type::negative) &&
			  (lexeme.type == Lexeme::Type::parR || lexeme.type == Lexeme::Type::squareBrR ||
			   lexeme.type == Lexeme::Type::alt)))) {
			//  We place eps between
			lexemes.emplace_back(Lexeme::Type::eps);
		}

		if (lexeme.type == Lexeme::Type::squareBrL) {
			memory_opening_indexes.push(lexemes.size());
		}

		lexemes.push_back(lexeme);
	}

	if (regex_is_eps || !brackets_checker.empty())
		return {Lexeme::Type::error};

	// проверка на отсутствие вложенных захватов памяти для одной ячейки
	unordered_set<int> opened_memory_cells;
	for (const auto& l : lexemes) {
		switch (l.type) {
		case Lexeme::Type::squareBrL:
			if (opened_memory_cells.count(l.number))
				return {Lexeme::Type::error};
			opened_memory_cells.insert(l.number);
			break;
		case Lexeme::Type::squareBrR:
			opened_memory_cells.erase(l.number);
			break;
		default:
			break;
		}
	}

	if (!lexemes.empty() && lexemes[0].type == Lexeme::Type::alt) {
		lexemes.insert(lexemes.begin(), {Lexeme::Type::eps});
	}

	if (lexemes.back().type == Lexeme::Type::alt || lexemes.back().type == Lexeme::Type::negative) {
		lexemes.emplace_back(Lexeme::Type::eps);
	}

	return lexemes;
}

bool AlgExpression::from_string(const string& str, bool allow_ref, bool allow_negation) {
	if (str.empty()) {
		value = Lexeme::Type::eps;
		type = Type::eps;
		alphabet = {};
		language = make_shared<Language>(alphabet);
		return true;
	}

	vector<Lexeme> l = parse_string(str, allow_ref, allow_negation);
	AlgExpression* root = expr(l, 0, l.size());

	if (root == nullptr || root->type == eps) {
		delete root;
		return false;
	}

	copy(root);
	language = make_shared<Language>(alphabet);

	delete root;
	return true;
}

void AlgExpression::update_balance(const AlgExpression::Lexeme& l, int& balance) {
	if (l.type == Lexeme::Type::parL || l.type == Lexeme::Type::squareBrL) {
		balance++;
	}
	if (l.type == Lexeme::Type::parR || l.type == Lexeme::Type::squareBrR) {
		balance--;
	}
}

AlgExpression* AlgExpression::scan_conc(const vector<AlgExpression::Lexeme>& lexemes,
										int index_start, int index_end) {
	AlgExpression* p = nullptr;
	int balance = 0;
	for (int i = index_start; i < index_end; i++) {
		update_balance(lexemes[i], balance);
		if (lexemes[i].type == Lexeme::Type::conc && balance == 0) {
			AlgExpression* l = expr(lexemes, index_start, i);
			AlgExpression* r = expr(lexemes, i + 1, index_end);
			if (l == nullptr || r == nullptr || r->type == AlgExpression::eps ||
				l->type == AlgExpression::eps) { // Проверка на адекватность)
				delete r;
				delete l;
				return p;
			}

			p = make();
			p->term_l = l;
			p->term_r = r;
			p->value = lexemes[i];
			p->type = conc;

			set<alphabet_symbol> s = l->alphabet;
			s.insert(r->alphabet.begin(), r->alphabet.end());
			p->alphabet = s;
			return p;
		}
	}
	return nullptr;
}

AlgExpression* AlgExpression::scan_star(const vector<AlgExpression::Lexeme>& lexemes,
										int index_start, int index_end) {
	AlgExpression* p = nullptr;
	int balance = 0;
	for (int i = index_start; i < index_end; i++) {
		update_balance(lexemes[i], balance);
		if (lexemes[i].type == Lexeme::Type::star && balance == 0) {
			AlgExpression* l = expr(lexemes, index_start, i);
			if (l == nullptr || l->type == AlgExpression::eps) {
				delete l;
				return p;
			}

			p = make();
			p->term_l = l;
			p->value = lexemes[i];
			p->type = Type::star;

			p->alphabet = l->alphabet;
			return p;
		}
	}
	return nullptr;
}

AlgExpression* AlgExpression::scan_alt(const vector<AlgExpression::Lexeme>& lexemes,
									   int index_start, int index_end) {
	AlgExpression* p = nullptr;
	int balance = 0;
	for (int i = index_start; i < index_end; i++) {
		update_balance(lexemes[i], balance);
		if (lexemes[i].type == Lexeme::Type::alt && balance == 0) {
			AlgExpression* l = expr(lexemes, index_start, i);
			AlgExpression* r = expr(lexemes, i + 1, index_end);
			if (l == nullptr || r == nullptr) { // Проверка на адекватность)
				delete r;
				delete l;
				return nullptr;
			}

			p = make();
			p->term_l = l;
			p->term_r = r;

			p->value = lexemes[i];
			p->type = Type::alt;

			p->alphabet = l->alphabet;
			p->alphabet.insert(r->alphabet.begin(), r->alphabet.end());
			return p;
		}
	}
	return nullptr;
}

AlgExpression* AlgExpression::scan_symb(const vector<AlgExpression::Lexeme>& lexemes,
										int index_start, int index_end) {
	AlgExpression* p = nullptr;
	if (index_start >= lexemes.size() || (index_end - index_start > 1) ||
		lexemes[index_start].type != Lexeme::Type::symb) {
		return nullptr;
	}

	p = make();
	p->value = lexemes[index_start];
	p->type = AlgExpression::symb;
	p->alphabet = {lexemes[index_start].symbol};
	return p;
}

AlgExpression* AlgExpression::scan_eps(const vector<AlgExpression::Lexeme>& lexemes,
									   int index_start, int index_end) {
	AlgExpression* p = nullptr;
	if (index_start >= lexemes.size() || (index_end - index_start != 1) ||
		lexemes[index_start].type != Lexeme::Type::eps) {
		return nullptr;
	}

	p = make();
	p->value = lexemes[index_start];
	p->type = AlgExpression::eps;
	return p;
}

AlgExpression* AlgExpression::scan_par(const vector<AlgExpression::Lexeme>& lexemes,
									   int index_start, int index_end) {
	AlgExpression* p = nullptr;
	if ((index_end - 1) >= lexemes.size() || (lexemes[index_start].type != Lexeme::Type::parL ||
											  lexemes[index_end - 1].type != Lexeme::Type::parR)) {
		return nullptr;
	}

	p = expr(lexemes, index_start + 1, index_end - 1);
	return p;
}

vector<AlgExpression*> AlgExpression::pre_order_travers() {
	vector<AlgExpression*> res;
	if (AlgExpression::symb == type) {
		res.push_back(this);
		return res;
	}

	if (term_l) {
		vector<AlgExpression*> l = term_l->pre_order_travers();
		res.insert(res.end(), l.begin(), l.end());
	}

	if (term_r) {
		vector<AlgExpression*> r = term_r->pre_order_travers();
		res.insert(res.end(), r.begin(), r.end());
	}

	return res;
}

bool AlgExpression::contains_eps() const {
	switch (type) {
	case Type::alt:
		return term_l->contains_eps() || term_r->contains_eps();
	case conc:
		return term_l->contains_eps() && term_r->contains_eps();
	case Type::star:
	case AlgExpression::eps:
		return true;
	default:
		return false;
	}
}

bool AlgExpression::equality_checker(const AlgExpression* expr1, const AlgExpression* expr2) {
	if (expr1 == nullptr && expr2 == nullptr)
		return true;
	if (expr1 == nullptr || expr2 == nullptr)
		return false;
	if (expr1->value.type != expr2->value.type)
		return false;

	if (expr1->value.type == Lexeme::Type::symb) {
		alphabet_symbol r1_symb, r2_symb;
		r1_symb = expr1->value.symbol;
		r2_symb = expr2->value.symbol;
		if (r1_symb != r2_symb)
			return false;
	}

	if (equality_checker(expr1->term_l, expr2->term_l) &&
		equality_checker(expr1->term_r, expr2->term_r))
		return true;
	if (equality_checker(expr1->term_r, expr2->term_l) &&
		equality_checker(expr1->term_l, expr2->term_r))
		return true;
	return false;
}

// для метода test
string AlgExpression::get_iterated_word(int n) const {
	string str;
	/*if (type == Type::alt) {
		cout << "ERROR: regex with '|' is passed to the method Test\n";
		return "";
	}*/
	if (term_l) {
		if (type == Type::star) {
			for (int i = 0; i < n; i++)
				str += term_l->get_iterated_word(n);
		} else
			str += term_l->get_iterated_word(n);
	}
	if (term_r && type != Type::alt) {
		str += term_r->get_iterated_word(n);
	}
	if (value.symbol != "") {
		str += value.symbol;
	}
	return str;
}

vector<AlgExpression::Lexeme> AlgExpression::first_state() const {
	vector<AlgExpression::Lexeme> l;
	vector<AlgExpression::Lexeme> r;
	switch (type) {
	case Type::alt:
		l = term_l->first_state();
		r = term_r->first_state();
		l.insert(l.end(), r.begin(), r.end());
		return l;
	case Type::star:
		l = term_l->first_state();
		return l;
	case Type::conc:
		l = term_l->first_state();
		if (term_l->contains_eps()) {
			r = term_r->first_state();
			l.insert(l.end(), r.begin(), r.end());
		}
		return l;
	case AlgExpression::eps:
		return {};
	default:
		l.push_back(value);
		return l;
	}
}

vector<AlgExpression::Lexeme> AlgExpression::end_state() const {
	vector<AlgExpression::Lexeme> l;
	vector<AlgExpression::Lexeme> r;
	switch (type) {
	case Type::alt:
		l = term_l->end_state();
		r = term_r->end_state();
		l.insert(l.end(), r.begin(), r.end());
		return l;
	case Type::star:
		l = term_l->end_state();
		return l;
	case Type::conc:
		l = term_r->end_state();
		if (term_r->contains_eps()) {
			r = term_l->end_state();
			l.insert(l.end(), r.begin(), r.end());
		}
		return l;
	case AlgExpression::eps:
		return {};
	default:
		l.push_back(value);
		return l;
	}
}

unordered_map<int, vector<int>> AlgExpression::pairs() const {
	unordered_map<int, vector<int>> l;
	unordered_map<int, vector<int>> r;
	unordered_map<int, vector<int>> p;
	vector<AlgExpression::Lexeme> rs;
	vector<AlgExpression::Lexeme> ps;
	switch (type) {
	case Type::alt:
		l = term_l->pairs();
		r = term_r->pairs();
		for (auto& it : r) {
			l[it.first].insert(l[it.first].end(), it.second.begin(), it.second.end());
		}
		return l;
	case Type::star:
		l = term_l->pairs();
		rs = term_l->end_state();
		ps = term_l->first_state();
		for (auto& i : rs) {
			for (auto& p : ps) {
				r[i.number].push_back(p.number);
			}
		}
		for (auto& it : r) {
			l[it.first].insert(l[it.first].end(), it.second.begin(), it.second.end());
		}
		return l;
	case Type::conc:
		l = term_l->pairs();
		r = term_r->pairs();
		for (auto& it : r) {
			l[it.first].insert(l[it.first].end(), it.second.begin(), it.second.end());
		}
		r = {};
		rs = term_l->end_state();
		ps = term_r->first_state();

		for (size_t i = 0; i < rs.size(); i++) {
			for (size_t j = 0; j < ps.size(); j++) {
				r[rs[i].number].push_back(ps[j].number);
			}
		}
		for (auto& it : r) {
			l[it.first].insert(l[it.first].end(), it.second.begin(), it.second.end());
		}
		return l;
	default:
		break;
	}
	return {};
}

void AlgExpression::regex_union(AlgExpression* a, AlgExpression* b) {
	type = Type::conc;
	term_l = a->make_copy();
	term_r = b->make_copy();
}

void AlgExpression::regex_alt(AlgExpression* a, AlgExpression* b) {
	type = Type::alt;
	term_l = a->make_copy();
	term_r = b->make_copy();
}

void AlgExpression::regex_star(AlgExpression* a) {
	type = Type::star;
	term_l = a->make_copy();
}

void AlgExpression::regex_eps() {
	type = Type::eps;
}