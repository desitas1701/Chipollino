#include "Objects/MemoryCommon.h"

Cell::Cell(int number, int lin_number) : number(number), lin_number(lin_number) {}

bool Cell::operator==(const Cell& other) const {
	return number == other.number && lin_number == other.lin_number;
}

size_t Cell::Hasher::operator()(const Cell& c) const {
	IntPairHasher hasher;
	return hasher({c.number, c.lin_number});
}

CellSet get_union(const CellSet& set1, const CellSet& set2) {
	CellSet result = set1;
	result.insert(set2.begin(), set2.end());
	return result;
}

CellSet get_intersection(const CellSet& set1, const CellSet& set2) {
	CellSet result;
	for (const auto& element : set1) {
		if (set2.find(element) != set2.end()) {
			result.insert(element);
		}
	}
	return result;
}

std::size_t CaptureGroup::State::Hasher::operator()(const State& s) const {
	IntPairHasher hasher;
	return hasher({s.index, s.class_num});
}

bool CaptureGroup::State::operator==(const State& other) const {
	return index == other.index && class_num == other.class_num;
}

CaptureGroup::CaptureGroup(int cell, const std::vector<std::vector<int>>& _traces,
						   const std::vector<int>& _state_classes)
	: cell(cell) {
	for (const auto& trace : _traces) {
		traces.insert(trace);
		for (auto st : trace) {
			states.insert({st, _state_classes[st]});
			state_classes.insert(_state_classes[st]);
		}
	}
}

bool CaptureGroup::operator==(const CaptureGroup& other) const {
	return cell == other.cell && states == other.states;
}

std::unordered_set<int> CaptureGroup::get_states_diff(
	const std::unordered_set<int>& other_state_classes) const {
	std::unordered_set<int> res;
	for (auto st : states)
		if (!other_state_classes.count(st.class_num))
			res.insert(st.index);

	for (const auto& trace : traces)
		for (int i = trace.size() - 1; i > 0; i--)
			if (res.count(trace[i - 1]))
				res.insert(trace[i]);
	return res;
}

std::ostream& operator<<(std::ostream& os, const CaptureGroup& cg) {
	os << "{\n";
	for (const auto& i : cg.traces)
		os << i;
	os << "}\n";
	for (const auto& i : cg.states)
		os << "{" << i.index << ": " << i.class_num << "} ";
	return os << "\n";
}
