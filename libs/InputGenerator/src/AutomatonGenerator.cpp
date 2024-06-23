#include "InputGenerator/AutomatonGenerator.h"

using std::ofstream;
using std::rand;
using std::set;
using std::string;
using std::to_string;
using std::vector;

void AutomatonGenerator::change_seed() {
	seed_it++;
	srand((size_t)time(nullptr) + seed_it + rand());
}

bool AutomatonGenerator::dice_throwing(int percentage) {
	change_seed();
	if (percentage > rand() % 100) {
		return true;
	}

	return false;
}

void AutomatonGenerator::add_terminality() {
	std::vector<int> unreachable;

	while (true) {
		unreachable.clear();
		for (int i = 0; i < states_number; i++)
			if (!finality_coloring[i])
				unreachable.push_back(i);
		if (unreachable.empty())
			break;
		int vertex = unreachable[rand() % unreachable.size()];
		stateDescriptions[vertex].final = true;
		std::queue<int> bfs;
		bfs.push(vertex);
		finality_coloring[vertex] = true;
		while (!bfs.empty()) {
			for (int i = 0; i < regraph[bfs.front()].size(); i++) {
				if (!finality_coloring[regraph[bfs.front()][i]]) {
					finality_coloring[regraph[bfs.front()][i]] = true;
					bfs.push(regraph[bfs.front()][i]);
				}
			}
			bfs.pop();
		}
	}
	for (int i = 0; i < states_number; i++) {
		if (!stateDescriptions[i].final && dice_throwing(final_probability))
			stateDescriptions[i].final = true;
	}
}

bool AutomatonGenerator::coloring_MFA_transition(int beg, FAtransition& trans, int color) {
	// We are coloring white vertex with red and making an edge with memory reopening
	if (trans.end == beg && MFA_coloring[color][trans.end] == 0)
		return false;
	// red
	if (MFA_coloring[color][trans.end] == 1)
		return false;
	// set color memory cell idle
	trans.close.erase(color);
	// not yellow
	if (MFA_coloring[color][beg] != 2) {
		MFA_coloring[color][beg] = 1;
		trans.open.insert(color);
	}
	// closing color memory cell for all transitions from the end vertex if it's not done yet
	if (MFA_coloring[color][trans.end] != 2) {
		for (auto& i : graph[trans.end]) {
			i.close.insert(color);
		}
	}
	// yellow
	MFA_coloring[color][trans.end] = 2;
	return true;
}

void AutomatonGenerator::generate_symbol(int beg, FAtransition& trans) {
	vector<int> possible_colors;
	for (int color = 0; color < colors; color++) {
		if (!trans.open.count(color) && MFA_coloring[color][beg] != 2)
			possible_colors.push_back(color);
	}

	if (dice_throwing(ref_probability) && !possible_colors.empty()) {
		trans.symbol = Symbol::Ref(possible_colors[rand() % possible_colors.size()]);
	} else {
		if (dice_throwing(epsilon_probability)) {
			trans.symbol = Symbol::Epsilon;
		} else {
			trans.symbol = alphabet[rand() % alphabet.size()];
			if (attributes.count("DFA")) {
				bool checked = false;
				while (!checked) {
					trans.symbol = alphabet[rand() % alphabet.size()];
					checked = true;
					for (auto& i : graph[beg]) {
						if (trans.symbol == i.symbol) {
							checked = false;
							break;
						}
					}
				}
			}
		}
	}
}

void AutomatonGenerator::generate_graph() {
	max_edges_number = states_number * (states_number - 1) / 2 + 3;
	edges_number = rand() % (max_edges_number - states_number + 1) + states_number - 1;

	graph.resize(states_number);

	std::vector<int> included_states, excluded_states;

	included_states.push_back(initial);
	for (int i = 1; i < states_number; i++) {
		excluded_states.push_back(i);
	}

	for (int i = 0; i < edges_number; i++) {
		FAtransition cur;
		int beg = included_states[rand() % included_states.size()];
		cur.end = included_states[rand() % included_states.size()];
		if (!excluded_states.empty()) {
			int ind = rand() % excluded_states.size();
			cur.end = excluded_states[ind];
			included_states.push_back(excluded_states[ind]);
			excluded_states.erase(excluded_states.begin() + ind);
		}
		//		cur.pop = "$";

		graph[beg].push_back(cur);
	}

	for (int i = 0; i < states_number; i++) {
		stateDescriptions.emplace_back(i, 0, i == initial);
	}

	regraph.resize(states_number);
	for (int i = 0; i < states_number; i++) {
		for (int j = 0; j < graph[i].size(); j++) {
			regraph[graph[i][j].end].push_back(i);
		}
	}

	add_terminality();

	for (int i = 0; i < colors_tries; i++) {
		int color = rand() % colors;
		int beg = included_states[rand() % included_states.size()];
		while (graph[beg].empty())
			beg = included_states[rand() % included_states.size()];
		int transition_num = rand() % graph[beg].size();
		coloring_MFA_transition(beg, graph[beg][transition_num], color);
	}

	for (int i = 0; i < states_number; i++) {
		for (int j = 0; j < graph[i].size(); j++) {
			generate_symbol(i, graph[i][j]);
		}
	}
}

void AutomatonGenerator::generate_alphabet(int max_alphabet_size) {
	change_seed();
	max_alphabet_size = max_alphabet_size > 26 ? 26 : max_alphabet_size;
	int alphabet_size = 0;
	if (max_alphabet_size)
		alphabet_size = rand() % max_alphabet_size + 1;

	for (char i = 'a'; i < 'a' + alphabet_size && i <= 'z'; i++) {
		alphabet.push_back(i);
	}
	// Символы, приходящие из regex - малые латинские буквы
	// for (char i = 'A'; i < 'A' + alphabet_size - 26 && i <= 'Z'; i++) {
	//     alphabet.push_back(i);
	// }
}

void AutomatonGenerator::write_to_file(const string& filename) {
	ofstream out;
	out.open(filename, ofstream::trunc);
	if (out.is_open())
		out << output.str();
	out.close();
}

bool AutomatonGenerator::parse_reserved(const std::string& res_case) {
	if (res_case == "EPS")
		return true;

	if (res_case == "LETTER") {
		if (!LETTER.empty()) {
			output << " " << LETTER.front();
			LETTER.pop();
			return true;
		}
		return false;
	}
	if (res_case == "DIGIT") {
		if (!DIGIT.empty()) {
			output << " " << DIGIT.front();
			DIGIT.pop();
			return true;
		}
		return false;
	}
	if (res_case == "STRING") {
		if (!STRING.empty()) {
			output << " " << STRING.front();
			STRING.pop();
			return true;
		}
		return false;
	}
	if (res_case == "NUMBER") {
		if (!NUMBER.empty()) {
			output << " " << NUMBER.front();
			NUMBER.pop();
			return true;
		}
		return false;
	}
	return false;
}

bool AutomatonGenerator::parse_nonterminal(lexy::_pt_node<lexy::_bra, void> ref) {
	return parse_transition(Parser::first_child(ref));
}

bool AutomatonGenerator::parse_terminal(lexy::_pt_node<lexy::_bra, void> ref) {
	auto it = ref.children().begin();
	it++;
	std::string to_read = lexy::as_string<string, lexy::ascii_encoding>(it->token().lexeme());
	auto res = (!TERMINAL.empty() && TERMINAL.front() == to_read);
	if (res) {
		output << " " << TERMINAL.front();
		TERMINAL.pop();
	}
	return res;
}

void AutomatonGenerator::parse_attribute(lexy::_pt_node<lexy::_bra, void> ref) {
	auto it = ref.children().begin();
	while (std::string(it->kind().name()) != "nonterminal")
		it++;
	attributes.insert(Parser::first_child(it));
}

bool AutomatonGenerator::parse_alternative(lexy::_pt_node<lexy::_bra, void> ref) {
	bool read = true;
	for (auto it = ref.children().begin(); it != ref.children().end(); it++) {
		if (it->kind().is_token() &&
			lexy::as_string<string, lexy::ascii_encoding>(it->token().lexeme()) == "|") {
			if (read)
				return true;
			read = true;
			continue;
		}
		if (!read) {
			continue;
		}
		if (it->kind().is_token() &&
			lexy::as_string<string, lexy::ascii_encoding>(it->token().lexeme()) == "(") {
			it++;
			if (attributes.count(Parser::first_child(it->children().begin()))) {
				while (it->kind().is_token() || std::string(it->kind().name()) != "alternative")
					it++;
				parse_alternative(*it);
			} else {
				while (!it->kind().is_token() ||
					   lexy::as_string<string, lexy::ascii_encoding>(it->token().lexeme()) != ":")
					it++;

				while (it->kind().is_token() || std::string(it->kind().name()) != "alternative")
					it++;
				parse_alternative(*it);
			}
			while (it->kind().is_token() &&
				   lexy::as_string<string, lexy::ascii_encoding>(it->token().lexeme()) != ")")
				it++;
		}
		if (std::string(it->kind().name()) == "terminal") {
			read &= parse_terminal(*it);
			continue;
		}
		if (std::string(it->kind().name()) == "nonterminal") {
			read &= parse_nonterminal(*it);
			continue;
		}
		if (std::string(it->kind().name()) == "reserved") {
			parse_reserved(Parser::first_child(it));
			continue;
		}
		if (std::string(it->kind().name()) == "attribute") {
			parse_attribute(*it);
			continue;
		}
	}
	return read;
}

bool AutomatonGenerator::parse_transition(const std::string& name) {

	if (!rewriting_rules.count(name)) {
		return false;
	}

	if (!parse_func.count(name)) {
		auto transition = rewriting_rules[name];
		return parse_alternative(*transition);
	}

	return parse_func[name]();
}

void AutomatonGenerator::setup_and_generate(FA_type type, const std::string& grammar_file) {
	generate_alphabet(52);

	lexy_ascii_tree grammar;

	auto file = lexy::read_file<lexy::ascii_encoding>(grammar_file.c_str());
	auto input = file.buffer();
	Lexer::parse_buffer(grammar, input);

	auto transitions = Parser::find_children(grammar, {"transition"}, {});
	for (auto transition : transitions) {
		// итератор по описанию перехода
		auto it = transition.children().begin();
		// имя нетерминала
		std::string nonterminal = Parser::first_child(it);
		while (std::string(it->kind().name()) != "alternative") {
			it++;
		}
		rewriting_rules[nonterminal] = it;
	}

	switch (type) {
	case FA_type::MFA:
		TERMINAL.emplace("MFA");
		break;
	case FA_type::NFA:
		TERMINAL.emplace("NFA");
		colors_tries = 0;
		colors = 0;
		break;
	case FA_type::DFA:
		TERMINAL.emplace("DFA");
		colors_tries = 0;
		colors = 0;
		epsilon_probability = 0;
		break;
	}

	generate_graph();
	if (!parse_transition("production"))
		throw std::runtime_error(
			"AutomatonGenerator: can not apply grammar for generated automaton");
}

AutomatonGenerator::AutomatonGenerator(FA_type type, int n, const std::string& grammar_file)
	: states_number(n) {
	setup_and_generate(type, grammar_file);
}

AutomatonGenerator::AutomatonGenerator(const AutomatonGeneratorBuilder& builder)
	: states_number(builder.states_number_), colors(builder.colors_),
	  colors_tries(builder.colors_tries_), final_probability(builder.final_probability_),
	  epsilon_probability(builder.epsilon_probability_), ref_probability(builder.ref_probability_),
	  seed_it(builder.seed_it_), memory_cells_number(builder.memory_cells_number_) {
	setup_and_generate(builder.type_, builder.grammar_file_);
}
