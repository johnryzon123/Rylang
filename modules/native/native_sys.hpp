#include <chrono>
#include <iostream>
#include "colors.h"
#include "common.h"
#include "func.h"

namespace Frontend {
	class RyExit : public RyCallable {
	public:
		// exit() or exit(code)
		int arity() override { return 1; }

		RyValue call(Interpreter &interpreter, const std::vector<RyValue> arguments) override {
			int exitCode = 0;
			if (arguments[0].isNumber()) {
				exitCode = static_cast<int>(arguments[0].asNumber());
			}

			std::cout << RyColor::YELLOW << "[Ry] Process finished with exit code " << exitCode << std::endl
								<< RyColor::RESET;
			std::exit(exitCode);
		}
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<native fn exit>"; };
	};
	class ClockCallable : public RyCallable {
	public:
		int arity() override { return 0; }
		RyValue call(Interpreter &interp, std::vector<RyValue> args) override {
			auto now = std::chrono::steady_clock::now().time_since_epoch();
			double seconds = std::chrono::duration<double>(now).count();
			return RyValue(seconds);
		}
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<native fn clock>"; };
	};
	class RyClear : public RyCallable {
	public:
		int arity() override { return 0; } // clear() takes no arguments

		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override {
#ifdef _WIN32
			// Windows specific clear
			std::system("cls");
#else
			// Linux/macOS standard clear
			std::system("clear");
#endif
			return nullptr;
		}

		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}

		std::string toString() override { return "<native fn clear>"; }
	};
} // namespace Frontend
