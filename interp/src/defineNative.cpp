#include "common.h"
#include "native_io.hpp"
#include "native_list.hpp"
#include "native_sys.hpp"
#include "native_type.hpp"
#include "native_use.hpp"
#include "runtime.h"

namespace Frontend {
	/**
	 * Constructor for the Interpreter.
	 *
	 * @param environment The Environment object which contains all defined variables, functions, and modules in the
	 * current scope.
	 * @param globals The Environment object which contains all global variables, functions, and modules.
	 */
	Interpreter::Interpreter() {
		globals = std::make_shared<Environment>();
		environment = globals;

		// Native Functions
		// out()
		auto outFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyOut>());
		globals->define("out", outFn);

		// exit()
		auto exitFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyExit>());
		globals->define("exit", exitFn);

		// input
		auto inputFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyInput>());
		globals->define("input", inputFn);

		// array length
		auto lenFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyLen>());
		globals->define("len", lenFn);

		// pop()
		auto popFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyPop>());
		globals->define("pop", popFn);

		// use()
		auto useFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyUse>());
		globals->define("use", useFn);

		// type()
		auto typeFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyType>());
		globals->define("type", typeFn);

		// clock()
		auto clockFn = std::static_pointer_cast<RyCallable>(std::make_shared<ClockCallable>());
		globals->define("clock", clockFn);

		// clear()
		auto clearFn = std::static_pointer_cast<RyCallable>(std::make_shared<RyClear>());
		globals->define("clear", clearFn);
	}
} // namespace Frontend
