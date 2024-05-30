#include <iostream>

#include "Interpreter/Interpreter.h"

int main(int argc, char* argv[]) {
	// Приветствие
	std::cout << "Chipollino :-)\n";

	// Инициализируем интерпретатор
	Interpreter interpreter;
	interpreter.set_log_mode(Interpreter::LogMode::all);

	// Загружаем в интерпретатор файл с коммандами
	std::string load_file = "test.txt";
	std::string user_name = "";
	if (argc > 1)
		load_file = argv[1];
	if (argc > 2)
		user_name = argv[2];
	if (interpreter.run_file(user_name + load_file, user_name)) {
		interpreter.generate_log("./resources/" + user_name + "report.tex");
	} else {
		exit(1);
	}
}