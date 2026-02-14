#include <fstream>
#include <iostream>
#include <string>
#include "colors.h"
#include "interp/include/runtime.h"
#include "tools.h"

using namespace Frontend;

void runREPL(void *interpreter, void *resolver) {
	std::string line;
	std::string buffer;
	int indentLevel = 0;
	std::cout << "Ry (Ry's for You) REPL 0.1.0\n";

	while (true) {
		// Dynamic Prompt Logic
		if (buffer.empty()) {
			std::cout << RyColor::BLUE << ">> " << RyColor::RESET;
		} else {
			// Generate 4 dots for every level of indentation
			int dotCount = indentLevel * 4;
			std::cout << std::string(dotCount, '.') << " ";
		}

		if (!std::getline(std::cin, line))
			break;

		if (line.empty() && indentLevel > 0) {
			std::cout << RyColor::YELLOW << "(Input cancelled)\n" << RyColor::RESET;
			buffer.clear();
			indentLevel = 0;
			continue;
		}
		// Track nesting (ignoring strings/comments)
		int change = RyTools::countIndentation(line);
		indentLevel += change;

		buffer += line + "\n";


		// Execution Trigger
		// We only execute when we return to level 0 (all pairs closed)
		if (indentLevel <= 0) {
			if (line.compare("clear") == 0) {
				system("clear");
				buffer.clear();
				indentLevel = 0;
				reset_interpreter(interpreter, resolver);
				continue;
			} else if (!buffer.empty() && buffer != "\n") {
				try {
					run_source(interpreter, resolver, buffer.c_str()); // Run(buffer);
				} catch (const std::exception &e) {
					std::cerr << RyColor::RED << RyColor::BOLD << "Runtime Error: " << e.what() << std::endl << RyColor::RESET;
				}
			}
			buffer.clear();
			indentLevel = 0; // Hard reset to prevent negative drift
		}
	}
}

int main(int argc, char *argv[]) {
	void *interp = create_interpreter();
	void *resolver = create_resolver(interp);
	if (!interp) {
		std::cerr << RyColor::RED << RyColor::BOLD << "Failed to create interpreter\n" << RyColor::RESET;
		return 1;
	}
	if (!resolver) {
		std::cerr << RyColor::RED << RyColor::BOLD << "Failed to create resolver\n" << RyColor::RESET;
		destroy_resolver(resolver);
		destroy_interpreter(interp);
		return 1;
	}


	if (argc >= 2) {
		// ry run <file>
		if (std::string(argv[1]) == "run") {
			if (argc != 3) {
				std::cerr << RyColor::RED << RyColor::BOLD << "Usage: ry run <script>\n" << RyColor::RESET;
				destroy_resolver(resolver);
				destroy_interpreter(interp);
				return 1;
			}
			const char *path = argv[2];
			std::ifstream inputFile(path);
			if (!inputFile.is_open()) {
				std::cerr << RyColor::RED << RyColor::BOLD << "Could not open file: " << RyColor::RESET << path << "\n";
				destroy_resolver(resolver);
				destroy_interpreter(interp);
				return 1;
			}
			std::string src((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
			run_source(interp, resolver, src.c_str());
		}
		if (std::string(argv[1]) == "-v" || std::string(argv[1]) == "--version") {
			std::cout << "Ry version 0.1.0\n";
			return 0;
		}
		if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
			std::cout << RyColor::BOLD << "Ry (Ry's for You) Usage:\n" << RyColor::RESET;
			std::cout << "  ry              " << RyColor::CYAN << "Launch the REPL\n" << RyColor::RESET;
			std::cout << "  ry run <file>   " << RyColor::CYAN << "Run a .ry script\n" << RyColor::RESET;
			std::cout << "  ry -v           " << RyColor::CYAN << "Show version\n" << RyColor::RESET;
			return 0;
		}

	} else {
		runREPL(interp, resolver);
	}
	destroy_resolver(resolver);
	destroy_interpreter(interp);
	return 0;
}
