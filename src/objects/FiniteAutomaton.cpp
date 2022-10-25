#include "FiniteAutomaton.h"
#include "Language.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <stack>
using namespace std;

State::State() : index(0), is_terminal(false), identifier("") {}

State::State(int index, vector<int> label, string identifier, bool is_terminal,
			 map<alphabet_symbol, set<int>> transitions)
	: index(index), label(label), identifier(identifier),
	  is_terminal(is_terminal), transitions(transitions) {}

void State::set_transition(int to, alphabet_symbol symbol) {
	transitions[symbol].insert(to);
}

FiniteAutomaton::FiniteAutomaton() {}

FiniteAutomaton::FiniteAutomaton(int initial_state, Language* language,
								 vector<State> states, bool is_deterministic)
	: initial_state(initial_state), language(language), states(states),
	  is_deterministic(is_deterministic) {}

FiniteAutomaton::FiniteAutomaton(const FiniteAutomaton& other)
	: initial_state(other.initial_state), language(other.language),
	  states(other.states), is_deterministic(other.is_deterministic),
	  max_index(other.max_index) {}

string FiniteAutomaton::to_txt() const {
	stringstream ss;
	ss << "digraph {\n\trankdir = LR\n\tdummy [label = \"\", shape = none]\n\t";
	for (int i = 0; i < states.size(); i++) {
		ss << i << " [label = \"" << states[i].identifier << "\", shape = ";
		ss << (states[i].is_terminal ? "doublecircle]\n\t" : "circle]\n\t");
	}
	if (states.size() > initial_state)
		ss << "dummy -> " << states[initial_state].index << "\n";

	for (const auto& state : states) {
		for (const auto& elem : state.transitions) {
			for (int transition_to : elem.second) {
				ss << "\t" << state.index << " -> " << transition_to
				   << " [label = \"" << to_string(elem.first) << "\"]\n";
			}
		}
	}

	ss << "}\n";
	return ss.str();
}

//обход автомата в глубину
void dfs(vector<State> states, const set<alphabet_symbol>& alphabet, int index,
		 vector<int>& reachable, bool use_epsilons_only) {
	if (find(reachable.begin(), reachable.end(), index) == reachable.end()) {
		reachable.push_back(index);
		if (use_epsilons_only) {
			for (int transition_to : states[index].transitions[epsilon()]) {
				dfs(states, alphabet, transition_to, reachable,
					use_epsilons_only);
			}
		} else {
			for (alphabet_symbol symb : alphabet) {
				for (int transition_to : states[index].transitions[symb]) {
					dfs(states, alphabet, transition_to, reachable,
						use_epsilons_only);
				}
			}
		}
	}
}

vector<int> FiniteAutomaton::closure(const set<int>& indices,
									 bool use_epsilons_only) const {
	vector<int> reachable;
	for (int index : indices)
		dfs(states, language->get_alphabet(), index, reachable,
			use_epsilons_only);
	return reachable;
}

//проверка меток на равенство
bool belong(State q, State u) {
	if (q.label.size() != u.label.size()) return false;
	for (int i = 0; i < q.label.size(); i++) {
		if (q.label[i] != u.label[i]) return false;
	}
	return true;
}

FiniteAutomaton FiniteAutomaton::determinize() const {
	FiniteAutomaton dfa = FiniteAutomaton();
	vector<int> q0 = closure({0}, true);

	vector<int> label = q0;
	sort(label.begin(), label.end());
	dfa.initial_state = 0;
	string new_identifier;
	for (auto elem : label) {
		new_identifier +=
			(new_identifier.empty() ? "" : ", ") + states[elem].identifier;
	}
	State new_initial_state = {0, label, new_identifier, false,
							   map<alphabet_symbol, set<int>>()};
	dfa.states.push_back(new_initial_state);

	stack<vector<int>> s1;
	stack<int> s2;
	s1.push(q0);
	s2.push(0);

	while (!s1.empty()) {
		vector<int> z = s1.top();
		int index = s2.top();
		s1.pop();
		s2.pop();
		State q = dfa.states[index];

		for (int i : z) {
			if (states[i].is_terminal) {
				dfa.states[index].is_terminal = true;
				break;
			}
		}

		set<int> new_x;
		for (alphabet_symbol symb : language->get_alphabet()) {
			new_x.clear();
			for (int j : z) {
				auto transitions_by_symbol = states[j].transitions.find(symb);
				if (transitions_by_symbol != states[j].transitions.end())
					for (int k : transitions_by_symbol->second) {
						new_x.insert(k);
					}
			}

			vector<int> z1 = closure(new_x, true);
			vector<int> new_label = z1;
			sort(new_label.begin(), new_label.end());
			string new_identifier;
			for (auto elem : new_label) {
				new_identifier += (new_identifier.empty() ? "" : ", ") +
								  states[elem].identifier;
			}

			State q1 = {-1, new_label, new_identifier, false,
						map<alphabet_symbol, set<int>>()};
			bool accessory_flag = false;

			for (auto& state : dfa.states) {
				if (belong(q1, state)) {
					index = state.index;
					accessory_flag = true;
					break;
				}
			}

			if (!accessory_flag) index = -1;
			if (index != -1)
				q1 = dfa.states[index];
			else {
				index = dfa.states.size();
				q1.index = index;
				dfa.states.push_back(q1);
				s1.push(z1);
				s2.push(index);
			}
			dfa.states[q.index].transitions[symb].insert(q1.index);
		}
	}
	dfa.language = language;
	dfa.is_deterministic = true;
	return dfa;
}

FiniteAutomaton FiniteAutomaton::minimize() const {
	const optional<FiniteAutomaton>& language_min_dfa = language->get_min_dfa();
	if (language->get_min_dfa()) return *language_min_dfa;
	// минимизация
	FiniteAutomaton dfa = determinize();
	vector<bool> table(dfa.states.size() * dfa.states.size());
	int counter = 1;
	for (int i = 1; i < dfa.states.size(); i++) {
		for (int j = 0; j < counter; j++) {
			if (dfa.states[i].is_terminal ^ dfa.states[j].is_terminal) {
				table[i * dfa.states.size() + j] = true;
			}
		}
		counter++;
	}

	bool flag = true;
	while (flag) {
		counter = 1;
		flag = false;
		for (int i = 1; i < dfa.states.size(); i++) {
			for (int j = 0; j < counter; j++) {
				if (!table[i * dfa.states.size() + j]) {
					for (alphabet_symbol symb : language->get_alphabet()) {
						vector<int> to = {
							*dfa.states[i].transitions[symb].begin(),
							*dfa.states[j].transitions[symb].begin()};
						if (*dfa.states[i].transitions[symb].begin() <
							*dfa.states[j].transitions[symb].begin()) {
							to = {*dfa.states[j].transitions[symb].begin(),
								  *dfa.states[i].transitions[symb].begin()};
						}
						if (table[to[0] * dfa.states.size() + to[1]]) {
							table[i * dfa.states.size() + j] = true;
							flag = true;
						}
					}
				}
			}
			counter++;
		}
	}

	set<int> visited;
	vector<vector<int>> groups;
	counter = 1;
	for (int i = 1; i < dfa.states.size(); i++) {
		for (int j = 0; j < counter; j++) {
			if (!table[i * dfa.states.size() + j]) {
				groups.push_back({i, j});
				visited.insert(i);
				visited.insert(j);
			}
		}
		counter++;
	}

	counter = 1;
	for (int i = 0; i >= 0 && i < groups.size(); i++) {
		for (int j = counter; i >= 0 && j < groups.size(); j++) {
			bool in_first = false;
			bool in_second = false;
			if (find(groups[i].begin(), groups[i].end(), groups[j][0]) !=
				groups[i].end()) {
				in_first = true;
			}
			if (find(groups[i].begin(), groups[i].end(), groups[j][1]) !=
				groups[i].end()) {
				in_second = true;
			}
			if (in_first && in_second) {
				groups.erase(groups.begin() + j);
				i--;
				counter--;
				continue;
			}
			if (in_first) {
				groups[i].push_back(groups[j][1]);
				groups.erase(groups.begin() + j);
				i--;
				counter--;
			}
			if (in_second) {
				groups[i].push_back(groups[j][0]);
				groups.erase(groups.begin() + j);
				i--;
				counter--;
			}
		}
		counter++;
	}

	for (int i = 0; i < dfa.states.size(); i++) {
		if (find(visited.begin(), visited.end(), dfa.states[i].index) ==
			visited.end()) {
			groups.push_back({dfa.states[i].index});
		}
	}

	vector<int> classes(dfa.states.size());
	for (int i = 0; i < groups.size(); i++) {
		for (int j = 0; j < groups[i].size(); j++) {
			classes[groups[i][j]] = i;
		}
	}
	FiniteAutomaton minimized_dfa = dfa.merge_equivalent_classes(classes);
	// кэширование
	language->set_min_dfa(minimized_dfa);
	return minimized_dfa;
}

FiniteAutomaton FiniteAutomaton::remove_eps() const {
	FiniteAutomaton new_nfa;
	new_nfa.states = states;

	for (auto& state : new_nfa.states) {
		state.transitions = map<alphabet_symbol, set<int>>();
	}

	for (int i = 0; i < states.size(); i++) {
		vector<int> q = closure({states[i].index}, true);
		vector<set<int>> x;
		for (alphabet_symbol symb : language->get_alphabet()) {
			x.clear();
			for (int k : q) {
				auto transitions_by_symbol = states[k].transitions.find(symb);
				if (transitions_by_symbol != states[k].transitions.end())
					x.push_back(transitions_by_symbol->second);
			}
			vector<int> q1;
			set<int> x1;
			for (auto& k : x) {
				q1 = closure(k, true);
				for (int& m : q1) {
					x1.insert(m);
				}
			}
			for (auto elem : x1) {
				if (new_nfa.states[elem].is_terminal) {
					new_nfa.states[i].is_terminal = true;
				}
				new_nfa.states[i].transitions[symb].insert(elem);
			}
		}
	}
	new_nfa.initial_state = initial_state;
	new_nfa.language = language;
	new_nfa.is_deterministic = false;
	return new_nfa;
}

FiniteAutomaton FiniteAutomaton::intersection(const FiniteAutomaton& dfa1,
											  const FiniteAutomaton& dfa2,
											  Language* _language) {
	_language->set_alphabet(dfa1.language->get_alphabet());
	FiniteAutomaton new_dfa;
	new_dfa.initial_state = 0;
	new_dfa.language = _language;
	int counter = 0;
	for (auto& state1 : dfa1.states) {
		for (auto& state2 : dfa2.states) {
			string new_identifier;
			new_identifier = state1.identifier.empty() ? "" : state1.identifier;
			new_identifier +=
				(state2.identifier.empty() ? "" : ", " + state2.identifier);
			new_dfa.states.push_back({counter,
									  {state1.index, state2.index},
									  new_identifier,
									  state1.is_terminal && state2.is_terminal,
									  map<alphabet_symbol, set<int>>()});
			counter++;
		}
	}

	for (auto& state : new_dfa.states) {
		for (alphabet_symbol symb : new_dfa.language->get_alphabet()) {
			state.transitions[symb].insert(
				*dfa1.states[state.label[0]].transitions.at(symb).begin() *
					dfa2.states.size() +
				*dfa2.states[state.label[1]].transitions.at(symb).begin());
		}
	}
	return new_dfa.determinize();
}

FiniteAutomaton FiniteAutomaton::uunion(const FiniteAutomaton& dfa1,
										const FiniteAutomaton& dfa2,
										Language* _language) {
	_language->set_alphabet(dfa1.language->get_alphabet());
	FiniteAutomaton new_dfa;
	new_dfa.initial_state = 0;
	new_dfa.language = _language;
	int counter = 0;
	for (auto& state1 : dfa1.states) {
		for (auto& state2 : dfa2.states) {
			string new_identifier;
			new_identifier = state1.identifier.empty() ? "" : state1.identifier;
			new_identifier +=
				(state2.identifier.empty() ? "" : ", " + state2.identifier);
			new_dfa.states.push_back({counter,
									  {state1.index, state2.index},
									  new_identifier,
									  state1.is_terminal || state2.is_terminal,
									  map<alphabet_symbol, set<int>>()});
			counter++;
		}
	}

	for (auto& state : new_dfa.states) {
		for (alphabet_symbol symb : new_dfa.language->get_alphabet()) {
			state.transitions[symb].insert(
				*dfa1.states[state.label[0]].transitions.at(symb).begin() *
					dfa2.states.size() +
				*dfa2.states[state.label[1]].transitions.at(symb).begin());
		}
	}

	return new_dfa.determinize();
}

FiniteAutomaton FiniteAutomaton::difference(const FiniteAutomaton& dfa2,
											Language* _language) const {
	_language->set_alphabet(language->get_alphabet());
	FiniteAutomaton new_dfa;
	new_dfa.initial_state = 0;
	new_dfa.language = _language;
	int counter = 0;
	for (auto& state1 : states) {
		for (auto& state2 : dfa2.states) {
			string new_identifier;
			new_identifier = state1.identifier.empty() ? "" : state1.identifier;
			new_identifier +=
				(state2.identifier.empty() ? "" : ", " + state2.identifier);
			new_dfa.states.push_back({counter,
									  {state1.index, state2.index},
									  new_identifier,
									  state1.is_terminal && !state2.is_terminal,
									  map<alphabet_symbol, set<int>>()});
			counter++;
		}
	}

	for (auto& state : new_dfa.states) {
		for (alphabet_symbol symb : new_dfa.language->get_alphabet()) {
			state.transitions[symb].insert(
				*states[state.label[0]].transitions.at(symb).begin() *
					dfa2.states.size() +
				*dfa2.states[state.label[1]].transitions.at(symb).begin());
		}
	}

	return new_dfa.determinize();
}

FiniteAutomaton FiniteAutomaton::complement(Language* _language) const {
	_language->set_alphabet(language->get_alphabet());
	FiniteAutomaton new_dfa =
		FiniteAutomaton(initial_state, _language, states, is_deterministic);
	for (int i = 0; i < new_dfa.states.size(); i++) {
		new_dfa.states[i].is_terminal = !new_dfa.states[i].is_terminal;
	}
	return new_dfa;
}
FiniteAutomaton FiniteAutomaton::reverse(Language* _language) const {
	_language->set_alphabet(language->get_alphabet());
	FiniteAutomaton enfa =
		FiniteAutomaton(initial_state, _language, states, is_deterministic);
	for (auto& state : enfa.states) {
		state.index += 1;
	}
	int old_initial_state = enfa.initial_state + 1;
	enfa.states.insert(enfa.states.begin(),
					   {0, {0}, "S", false, map<alphabet_symbol, set<int>>()});
	enfa.initial_state = 0;
	for (int i = 1; i < enfa.states.size(); i++) {
		if (enfa.states[i].is_terminal) {
			enfa.states[initial_state].transitions[epsilon()].insert(
				enfa.states[i].index);
			enfa.states[i].is_terminal = false;
		}
	}
	enfa.states[old_initial_state].is_terminal = true;
	vector<map<alphabet_symbol, set<int>>> new_transition_matrix(
		enfa.states.size() - 1);
	for (int i = 1; i < enfa.states.size(); i++) {
		for (auto& transition : enfa.states[i].transitions) {
			for (int elem : transition.second) {
				new_transition_matrix[elem][transition.first].insert(
					enfa.states[i].index);
			}
		}
	}
	for (int i = 1; i < enfa.states.size(); i++) {
		enfa.states[i].transitions = new_transition_matrix[i - 1];
	}
	return enfa;
}

FiniteAutomaton FiniteAutomaton::add_trap_state() const {
	FiniteAutomaton new_dfa(*this);
	bool flag = true;
	int count = new_dfa.states.size();
	for (int i = 0; i < count; i++) {
		for (alphabet_symbol symb : language->get_alphabet()) {
			if (!new_dfa.states[i].transitions[symb].size()) {
				if (flag) {
					new_dfa.states[i].set_transition(new_dfa.states.size(),
													 symb);
					int size = new_dfa.states.size();
					new_dfa.states.push_back(
						{size,
						 {size},
						 to_string(size),
						 false,
						 map<alphabet_symbol, set<int>>()});
				} else {
					new_dfa.states[i].set_transition(new_dfa.states.size() - 1,
													 symb);
				}
				flag = false;
			}
		}
	}
	if (!flag) {
		for (alphabet_symbol symb : language->get_alphabet()) {
			new_dfa.states[new_dfa.states.size() - 1].transitions[symb].insert(
				new_dfa.states.size() - 1);
		}
	}
	return new_dfa;
}

FiniteAutomaton FiniteAutomaton::remove_trap_state() const {
	/*FiniteAutomaton new_dfa(*this);
	int count = new_dfa.states.size();
	for (int i = 0; i >= 0 && i < count; i++) {
		bool flag = false;
		for (auto& transitions : new_dfa.states[i].transitions) {
			for (int transition_to : transitions.second) {
				if (i == transition_to && !new_dfa.states[i].is_terminal) {
					flag = true;
				}
			}
		}
		if (flag) {
			new_dfa.states.erase(new_dfa.states.begin() + i);
			if (i != count - 1) {
				for (int j = new_dfa.states[i].index - 1;
					 j < new_dfa.states.size(); j++) {
					new_dfa.states[j].index -= 1;
				}
			}
			for (int j = 0; i >= 0 && j < new_dfa.states.size(); j++) {
				for (auto& transitions : new_dfa.states[j].transitions) {
					for (int transition_to : transitions.second) {
						if (transition_to == i) {
							transitions.second.erase(transition_to);
						}
						if (transition_to > i) {
							transitions.second[k] -= 1;
						}
					}
				}
			}
			i--;
			count--;
		}
	}
	return new_dfa;*/
	return FiniteAutomaton();
}

FiniteAutomaton FiniteAutomaton::merge_equivalent_classes(
	vector<int> classes) const {
	map<int, vector<int>>
		class_to_index; // нужен для подсчета количества классов
	for (int i = 0; i < classes.size(); i++)
		class_to_index[classes[i]].push_back(i);
	// индексы состояний в новом автомате соответсвуют номеру класса
	// эквивалентности
	vector<State> new_states;
	for (int i = 0; i < class_to_index.size(); i++) {
		string new_identifier;
		for (int index : class_to_index[i])
			new_identifier +=
				(new_identifier.empty() ? "" : ", ") + states[index].identifier;
		State s = {
			i, {i}, new_identifier, false, map<alphabet_symbol, set<int>>()};
		new_states.push_back(s);
	}

	for (int i = 0; i < states.size(); i++) {
		int from = classes[i];
		for (const auto& elem : states[i].transitions) {
			for (int transition_to : elem.second) {
				int to = classes[transition_to];
				new_states[from].transitions[elem.first].insert(to);
			}
		}
	}

	for (const auto& elem : class_to_index)
		if (states[elem.second[0]].is_terminal)
			new_states[elem.first].is_terminal = true;

	return FiniteAutomaton(classes[initial_state], language, new_states,
						   is_deterministic);
}
// структуры и функции для работы с грамматикой
struct GrammarItem {
	enum Type {
		terminal,
		nonterminal
	};
	Type type;
	int state_index, class_number;
	string term_name;
	GrammarItem()
		: type(terminal), state_index(-1), class_number(-1), term_name("") {}
	GrammarItem(Type type, int state_index, int class_number)
		: type(type), state_index(state_index), class_number(class_number) {}
	GrammarItem(Type type, string term_name)
		: type(type), term_name(term_name) {}
	bool operator!=(const GrammarItem& other) {
		return type != other.type || state_index != other.state_index ||
			   class_number != other.class_number ||
			   term_name != other.term_name;
	}
	void operator=(const GrammarItem& other) {
		type = other.type;
		state_index = other.state_index;
		class_number = other.class_number;
		term_name = other.term_name;
	}
};
// для отладки
ostream& operator<<(ostream& os, const GrammarItem& item) {
	if (item.type == GrammarItem::terminal) {
		if (item.term_name == "\0") return os << "eps";
		if (item.term_name == "\1") return os << "init";
		return os << item.term_name;
	} else
		return os << "S" << item.state_index;
}
// обновляю значение class_number для каждого нетерминала
void update_classes(set<int>& checker,
					map<set<string>, vector<GrammarItem*>>& classes_check_map) {
	int classNum = 0;
	checker.clear();
	for (const auto& elem : classes_check_map) {
		checker.insert(elem.second[0]->state_index);
		for (GrammarItem* nont : elem.second) {
			nont->class_number = classNum;
		}
		classNum++;
	}
}
// строю новые классы эквивалентности по терминальным формам
void check_classes(vector<vector<vector<GrammarItem*>>>& rules,
				   map<set<string>, vector<GrammarItem*>>& classes_check_map,
				   vector<GrammarItem*>& nonterminals) {
	classes_check_map.clear();
	for (int i = 0; i < nonterminals.size(); i++) {
		set<string> temp_rules;
		for (vector<GrammarItem*> rule : rules[i]) {
			string newRule;

			for (GrammarItem* t : rule) {
				if (t->type == GrammarItem::terminal)
					newRule += t->term_name;
				else
					newRule += to_string(t->class_number);
			}

			temp_rules.insert(newRule);
		}
		classes_check_map[temp_rules].push_back(nonterminals[i]);
	}
}

vector<vector<vector<GrammarItem*>>> get_bisimilar_grammar(
	vector<vector<vector<GrammarItem*>>>& rules,
	vector<GrammarItem*>& nonterminals,
	vector<GrammarItem*>& bisimilar_nonterminals) {
	map<set<string>, vector<GrammarItem*>> classes_check_map;
	set<int> checker;
	// checker
	while (true) {
		set<int> temp = checker;
		check_classes(rules, classes_check_map, nonterminals);
		update_classes(checker, classes_check_map);
		if (checker == temp) break;
	}
	/*for (auto& elem : classes_check_map) {
		cout << "{";
		for (int i = 0; i < elem.second.size() - 1; i++)
			cout << *elem.second[i] << ",";
		cout << *elem.second[elem.second.size() - 1] << "}";
	}
	cout << endl;*/
	// формирование бисимилярной грамматики
	map<int, GrammarItem*> class_to_nonterm;
	for (const auto& elem : classes_check_map)
		class_to_nonterm[elem.second[0]->class_number] = elem.second[0];
	vector<vector<vector<GrammarItem*>>> bisimilar_rules;
	for (const auto& elem : classes_check_map) {
		GrammarItem* curNonterm = elem.second[0];
		vector<vector<GrammarItem*>> temp_rules;
		for (vector<GrammarItem*> rule : rules[curNonterm->state_index]) {
			vector<GrammarItem*> tempRule;
			for (GrammarItem* item : rule) {
				if (item->type == GrammarItem::nonterminal) {
					tempRule.push_back(class_to_nonterm[item->class_number]);
				} else
					tempRule.push_back(item);
			}
			temp_rules.push_back(tempRule);
		}
		bisimilar_nonterminals.push_back(curNonterm);
		bisimilar_rules.push_back(temp_rules);
	}

	return bisimilar_rules;
}
// преобразование конечного автомата в грамматику
// в векторе терминалов по 0му индексу лежит epsilon, по terminals.size()-1
// лежит initial
vector<vector<vector<GrammarItem*>>> fa_to_grammar(
	const vector<State>& states, const set<alphabet_symbol>& alphabet,
	int initial_state, vector<GrammarItem>& fa_items,
	vector<GrammarItem*>& nonterminals, vector<GrammarItem*>& terminals) {
	vector<vector<vector<GrammarItem*>>> rules(states.size());
	fa_items.resize(states.size() + alphabet.size() + 2);
	int item_ind = 0;
	while (item_ind < states.size()) {
		fa_items[item_ind] = GrammarItem(GrammarItem::nonterminal, item_ind, 0);
		nonterminals.push_back(&fa_items[item_ind]);
		item_ind++;
	}
	map<alphabet_symbol, int> terminal_index;
	fa_items[item_ind] = (GrammarItem(GrammarItem::terminal, "\0"));
	terminals.push_back(&fa_items[item_ind]);
	terminal_index[epsilon()] = 0;
	item_ind++;
	for (alphabet_symbol symb : alphabet) {
		fa_items[item_ind] =
			(GrammarItem(GrammarItem::terminal, to_string(symb)));
		terminals.push_back(&fa_items[item_ind]);
		terminal_index[symb] = item_ind - nonterminals.size();
		item_ind++;
	}
	fa_items[item_ind] = (GrammarItem(GrammarItem::terminal,
									  "\1")); // обозначает начальное состояние
	terminals.push_back(&fa_items[item_ind]);

	for (int i = 0; i < states.size(); i++) {
		for (const auto& elem : states[i].transitions) {
			for (int transition_to : elem.second)
				rules[i].push_back({terminals[terminal_index[elem.first]],
									nonterminals[transition_to]});
		}
		if (states[i].is_terminal) rules[i].push_back({terminals[0]});
	}
	rules[initial_state].push_back({terminals[alphabet.size() + 1]});

	return rules;
}
// имя терминала - строка (тк может представлять собой число)
// нужно из имени получать alphabet_symbol
alphabet_symbol to_alphabet_symbol(string s) {
	if (s == "\0")
		return epsilon();
	else
		return s[0];
}
FiniteAutomaton FiniteAutomaton::merge_bisimilar() const {
	vector<GrammarItem> fa_items;
	vector<GrammarItem*> nonterminals;
	vector<GrammarItem*> terminals;

	vector<vector<vector<GrammarItem*>>> rules =
		fa_to_grammar(states, language->get_alphabet(), initial_state, fa_items,
					  nonterminals, terminals);

	vector<GrammarItem*> bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> bisimilar_rules =
		get_bisimilar_grammar(rules, nonterminals, bisimilar_nonterminals);
	vector<int> classes;
	for (const auto& nont : nonterminals)
		classes.push_back(nont->class_number);
	return merge_equivalent_classes(classes);
}

bool FiniteAutomaton::bisimilar(const FiniteAutomaton& fa1,
								const FiniteAutomaton& fa2) {
	// грамматики из автоматов
	vector<GrammarItem> fa1_items;
	vector<GrammarItem*> fa1_nonterminals;
	vector<GrammarItem*> fa1_terminals;
	vector<vector<vector<GrammarItem*>>> fa1_rules = fa_to_grammar(
		fa1.states, fa1.language->get_alphabet(), fa1.initial_state, fa1_items,
		fa1_nonterminals, fa1_terminals);

	vector<GrammarItem> fa2_items;
	vector<GrammarItem*> fa2_nonterminals;
	vector<GrammarItem*> fa2_terminals;
	vector<vector<vector<GrammarItem*>>> fa2_rules = fa_to_grammar(
		fa2.states, fa2.language->get_alphabet(), fa2.initial_state, fa2_items,
		fa2_nonterminals, fa2_terminals);

	if (fa1_terminals.size() != fa2_terminals.size()) return false;
	for (int i = 0; i < fa1_terminals.size(); i++)
		if (*fa1_terminals[i] != *fa2_terminals[i]) return false;
	// сначала получаем бисимилярные грамматики из данных автоматов
	vector<GrammarItem*> fa1_bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> fa1_bisimilar_rules =
		get_bisimilar_grammar(fa1_rules, fa1_nonterminals,
							  fa1_bisimilar_nonterminals);

	vector<GrammarItem*> fa2_bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> fa2_bisimilar_rules =
		get_bisimilar_grammar(fa2_rules, fa2_nonterminals,
							  fa2_bisimilar_nonterminals);
	if (fa1_bisimilar_nonterminals.size() != fa2_bisimilar_nonterminals.size())
		return false;
	// из объединения полученных ранее получаем итоговую
	vector<GrammarItem*> nonterminals(fa1_bisimilar_nonterminals);
	nonterminals.insert(nonterminals.end(), fa2_bisimilar_nonterminals.begin(),
						fa2_bisimilar_nonterminals.end());
	vector<vector<vector<GrammarItem*>>> rules(fa1_bisimilar_rules);
	rules.insert(rules.end(), fa2_bisimilar_rules.begin(),
				 fa2_bisimilar_rules.end());

	for (GrammarItem* nont : nonterminals)
		nont->class_number = 0; // сбрасываю номера классов

	vector<GrammarItem*> bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> bisimilar_rules =
		get_bisimilar_grammar(rules, nonterminals, bisimilar_nonterminals);

	if (fa1_bisimilar_nonterminals.size() != bisimilar_nonterminals.size())
		return false;

	return true;
}
// преобразование переходов автомата в грамматику (переход -> состояние переход)
vector<vector<vector<GrammarItem*>>> tansitions_to_grammar(
	const vector<State>& states, int initial_state,
	const vector<GrammarItem*>& fa_nonterminals,
	vector<pair<GrammarItem, map<alphabet_symbol, vector<GrammarItem>>>>&
		fa_items,
	vector<GrammarItem*>& nonterminals, vector<GrammarItem*>& terminals) {
	// fa_items вектор пар <терминал (состояние), map нетерминалов (переходов)>
	fa_items.resize(states.size());
	int ind = 0;
	for (int i = 0; i < states.size(); i++) {
		// имя терминала - класс  эквивалентности состояния
		fa_items[i].first = GrammarItem(
			GrammarItem::terminal, to_string(fa_nonterminals[i]->class_number));
		terminals.push_back(&fa_items[i].first);
		for (const auto& elem : states[i].transitions) {
			vector<GrammarItem>& item_vec = fa_items[i].second[elem.first];
			item_vec.resize(elem.second.size());
			for (int j = 0; j < elem.second.size(); j++) {
				item_vec[j] = (GrammarItem(GrammarItem::nonterminal, ind, 0));
				nonterminals.push_back(&item_vec[j]);
				ind++;
			}
		}
	}

	vector<vector<vector<GrammarItem*>>> rules(nonterminals.size());
	ind = 0;
	for (int i = 0; i < states.size(); i++) {
		for (const auto& elem : states[i].transitions) {
			for (int transition_to : elem.second) {
				// смотрим все переходы из состояния transition_to
				for (auto transition_elem : states[transition_to].transitions) {
					for (int k = 0; k < transition_elem.second.size(); k++) {
						int nonterm_ind = fa_items[transition_to]
											  .second[transition_elem.first][k]
											  .state_index;
						rules[ind].push_back({terminals[transition_to],
											  nonterminals[nonterm_ind]});
					}
				}
				if (states[transition_to].is_terminal)
					rules[ind].push_back(
						{terminals[transition_to]}); // переход в финальное
													 // состояние
				ind++;
			}
		}
	}

	return rules;
}
// построение обратной грамматики
vector<vector<vector<GrammarItem*>>> get_reverse_grammar(
	vector<vector<vector<GrammarItem*>>>& rules,
	vector<GrammarItem*>& nonterminals, vector<GrammarItem*>& terminals) {
	vector<vector<vector<GrammarItem*>>> reverse_rules(rules.size());
	for (int i = 0; i < rules.size(); i++) {
		for (int j = 0; j < rules[i].size(); j++) {
			if (rules[i][j].size() == 1) {
				if (rules[i][j][0]->term_name == "\1")
					reverse_rules[i].push_back({terminals[0]});
				if (rules[i][j][0]->term_name == "\0")
					reverse_rules[i].push_back(
						{terminals[terminals.size() - 1]});
			} else {
				reverse_rules[rules[i][j][1]->state_index].push_back(
					{rules[i][j][0], nonterminals[i]});
			}
		}
	}
	return reverse_rules;
}
bool FiniteAutomaton::equal(const FiniteAutomaton& fa1,
							const FiniteAutomaton& fa2) {
	// проверка равенства количества состояний
	if (fa1.states.size() != fa2.states.size()) return false;
	// грамматики из состояний автоматов
	vector<GrammarItem> fa1_items;
	vector<GrammarItem*> fa1_nonterminals;
	vector<GrammarItem*> fa1_terminals;
	vector<vector<vector<GrammarItem*>>> fa1_rules = fa_to_grammar(
		fa1.states, fa1.language->get_alphabet(), fa1.initial_state, fa1_items,
		fa1_nonterminals, fa1_terminals);

	vector<GrammarItem> fa2_items;
	vector<GrammarItem*> fa2_nonterminals;
	vector<GrammarItem*> fa2_terminals;
	vector<vector<vector<GrammarItem*>>> fa2_rules = fa_to_grammar(
		fa2.states, fa2.language->get_alphabet(), fa2.initial_state, fa2_items,
		fa2_nonterminals, fa2_terminals);

	// проверка на равенство алфавитов
	if (fa1_terminals.size() != fa2_terminals.size()) return false;
	for (int i = 0; i < fa1_terminals.size(); i++)
		if (*fa1_terminals[i] != *fa2_terminals[i]) return false;

	// биективная бисимуляция состояний
	vector<GrammarItem*> nonterminals(fa1_nonterminals);
	nonterminals.insert(nonterminals.end(), fa2_nonterminals.begin(),
						fa2_nonterminals.end());
	vector<vector<vector<GrammarItem*>>> rules(fa1_rules);
	rules.insert(rules.end(), fa2_rules.begin(), fa2_rules.end());
	for (GrammarItem* nont : nonterminals)
		nont->class_number = 0; // сбрасываю номера классов
	vector<GrammarItem*> bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> bisimilar_rules =
		get_bisimilar_grammar(rules, nonterminals, bisimilar_nonterminals);

	vector<int> classes(bisimilar_nonterminals.size(), 0);
	for (GrammarItem* nont : fa1_nonterminals)
		classes[nont->class_number]++;
	for (GrammarItem* nont : fa2_nonterminals)
		classes[nont->class_number]--;
	for (auto t : classes)
		if (t != 0) return false;

	vector<int> bisimilar_classes;
	for (GrammarItem* nont : nonterminals)
		bisimilar_classes.push_back(nont->class_number);

	// биективная бисимуляция обратных грамматик
	vector<vector<vector<GrammarItem*>>> fa1_reverse_rules =
		get_reverse_grammar(fa1_rules, fa1_nonterminals, fa1_terminals);
	vector<vector<vector<GrammarItem*>>> fa2_reverse_rules =
		get_reverse_grammar(fa2_rules, fa2_nonterminals, fa2_terminals);

	vector<vector<vector<GrammarItem*>>> reverse_rules(fa1_reverse_rules);
	reverse_rules.insert(reverse_rules.end(), fa2_reverse_rules.begin(),
						 fa2_reverse_rules.end());
	for (GrammarItem* nont : nonterminals)
		nont->class_number = 0; // сбрасываю номера классов

	vector<GrammarItem*> reverse_bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> reverse_bisimilar_rules =
		get_bisimilar_grammar(reverse_rules, nonterminals,
							  reverse_bisimilar_nonterminals);
	// сопоставление состояний 1 к 1
	vector<int> reverse_bisimilar_classes;
	for (GrammarItem* nont : nonterminals) {
		reverse_bisimilar_classes.push_back(nont->class_number);
		nont->class_number = -1;
	}

	/*for (auto t : bisimilar_classes)
		cout << t << " ";
	cout << endl;
	for (auto t : reverse_bisimilar_classes)
		cout << t << " ";
	cout << endl;*/

	int new_class = 0;
	for (int i = 0; i < bisimilar_classes.size(); i++) {
		if (nonterminals[i]->class_number != -1) continue;
		nonterminals[i]->class_number = new_class;
		vector<int> equiv_nont; // нетерминалы с таким же классом, что и i-й
		for (int j = i + 1; j < bisimilar_classes.size(); j++) {
			if (bisimilar_classes[j] == bisimilar_classes[i])
				equiv_nont.push_back(j);
		}
		for (int ind : equiv_nont)
			if (reverse_bisimilar_classes[ind] == reverse_bisimilar_classes[i])
				nonterminals[ind]->class_number = new_class;
		new_class++;
	}

	/*for (auto t : nonterminals)
		cout << t->class_number << " ";
	cout << endl;*/

	// грамматики из переходов
	vector<pair<GrammarItem, map<alphabet_symbol, vector<GrammarItem>>>>
		transitions1_items;
	vector<GrammarItem*> transitions1_nonterminals;
	vector<GrammarItem*> transitions1_terminals;
	vector<vector<vector<GrammarItem*>>> transitions1_rules =
		tansitions_to_grammar(fa1.states, fa1.initial_state, fa1_nonterminals,
							  transitions1_items, transitions1_nonterminals,
							  transitions1_terminals);

	/*for (int i = 0; i < transitions1_rules.size(); i++) {
		cout << *transitions1_nonterminals[i] << ": ";
		for (int j = 0; j < transitions1_rules[i].size(); j++) {
			for (int k = 0; k < transitions1_rules[i][j].size(); k++) {
				cout << *transitions1_rules[i][j][k];
			}
			cout << " ";
		}
		cout << endl;
	}*/

	vector<pair<GrammarItem, map<alphabet_symbol, vector<GrammarItem>>>>
		transitions2_items;
	vector<GrammarItem*> transitions2_nonterminals;
	vector<GrammarItem*> transitions2_terminals;
	vector<vector<vector<GrammarItem*>>> transitions2_rules =
		tansitions_to_grammar(fa2.states, fa2.initial_state, fa2_nonterminals,
							  transitions2_items, transitions2_nonterminals,
							  transitions2_terminals);

	// проверка равенства количества переходов
	if (transitions1_nonterminals.size() != transitions2_nonterminals.size())
		return false;
	// биективная бисимуляция переходов
	vector<GrammarItem*> transitions_nonterminals(transitions1_nonterminals);
	transitions_nonterminals.insert(transitions_nonterminals.end(),
									transitions2_nonterminals.begin(),
									transitions2_nonterminals.end());
	vector<vector<vector<GrammarItem*>>> transitions_rules(transitions1_rules);
	transitions_rules.insert(transitions_rules.end(),
							 transitions2_rules.begin(),
							 transitions2_rules.end());
	for (GrammarItem* nont : transitions_nonterminals)
		nont->class_number = 0; // сбрасываю номера классов
	vector<GrammarItem*> transitions_bisimilar_nonterminals;
	vector<vector<vector<GrammarItem*>>> transitions_bisimilar_rules =
		get_bisimilar_grammar(transitions_rules, transitions_nonterminals,
							  transitions_bisimilar_nonterminals);

	classes.clear();
	classes.resize(transitions_bisimilar_nonterminals.size(), 0);
	for (auto t : transitions1_nonterminals)
		classes[t->class_number]++;
	for (auto t : transitions2_nonterminals)
		classes[t->class_number]--;
	for (auto t : classes)
		if (t != 0) return false;

	return true;
}

bool FiniteAutomaton::equivalent(const FiniteAutomaton& fa1,
								 const FiniteAutomaton& fa2) {
	return equal(fa1.minimize(), fa2.minimize());
}

bool FiniteAutomaton::subset(const FiniteAutomaton& fa) const {
	FiniteAutomaton dfa1 = determinize();
	FiniteAutomaton dfa2 = fa.determinize();
	Language l;
	FiniteAutomaton dfa_instersection(intersection(dfa1, dfa2, &l));
	return equivalent(dfa_instersection, dfa2); // TODO
}
