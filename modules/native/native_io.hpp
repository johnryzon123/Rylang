#include <algorithm>
#include <string>
#include <iostream>
#include "runtime.h"

namespace Frontend {
	class RyOut : public RyCallable {
	private:
		void printValue(const RyValue &v) {
			if (v.isList()) {
				auto vec = v.asList();
				std::cout << "[";
				for (size_t i = 0; i < vec->size(); ++i) {
					printValue((*vec)[i]); // RECURSION
					if (i < vec->size() - 1)
						std::cout << ", ";
				}
				std::cout << "]";
			} else if (v.isNumber()) {
				std::cout << v.asNumber();
			} else if (v.isString()) {
				std::cout << v.asString();
			} else if (v.isBool()) {
				std::cout << (v.isBool() ? "true" : "false");
			} else if (v.isNil()) {
				std::cout << "null";
			} else {
				std::cout << "<object>";
			}
		}

	public:
		// out(x) takes exactly 1 argument
		int arity() override { return 1; }
		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override {
			if (arguments.empty()) {
				std::cout << std::endl;
				return nullptr;
			}

			for (size_t i = 0; i < arguments.size(); ++i) {
				RyValue value = arguments[i];

				// Handle function printing
				if (value.isFunction()) {
					auto fn = value.asFunction();
					std::cout << fn->toString();
				} else {
					printValue(value);
				}

				// Space between multiple arguments: out(a, b)
				if (i < arguments.size() - 1)
					std::cout << " ";
			}

			std::cout << std::endl; // Final newline after the whole output
			return nullptr;
		}
		virtual std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<native fn out>"; }
	};

	class RyInput : public RyCallable {
	public:
		int arity() override { return -1; }
		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override {
			// Handle the Prompt
			if (!arguments.empty()) {
				RyValue promptVal = arguments[0];
				if (promptVal.isString()) {
					std::cout << promptVal.asString();
				} else if (promptVal.isNumber()) {
					std::cout << promptVal.asNumber();
				}
				std::cout.flush(); // Ensure the prompt actually appears before waiting
			}

			if (std::cin.peek() == '\n')
				std::cin.ignore();

			std::string line;
			if (!std::getline(std::cin, line))
				return RyValue(nullptr);

			// Trim whitespace
			auto first = line.find_first_not_of(" \t\r\n");
			if (first == std::string::npos)
				return RyValue(std::string());
			auto last = line.find_last_not_of(" \t\r\n");
			std::string s = line.substr(first, last - first + 1);

			// Quoted string -> strip quotes
			if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
				return RyValue(s.substr(1, s.size() - 2));
			}

			// Lowercase copy for keyword checks
			std::string low = s;
			std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
			if (low == "true")
				return RyValue(true);
			if (low == "false")
				return RyValue(false);
			if (low == "null" || low == "nil")
				return RyValue(nullptr);

			// Try parse as number (ensure full consumption)
			try {
				size_t idx = 0;
				double val = std::stod(s, &idx);
				if (idx == s.size())
					return RyValue(val);
			} catch (...) {
				// not a number, fall through to string
			}

			// Default: return as string
			return RyValue(s);
		}
		virtual std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<native fn input>"; }
	};
} // namespace Frontend
