#include <string>
#include "func.h"
#include "runtime.h"

namespace Frontend {
	class RyType : public RyCallable {
	public:
		int arity() override { return 1; } // It takes one argument: type(x)

		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override {
			RyValue value = arguments[0];

			if (value.isNumber())
				return std::string("number");
			if (value.isString())
				return std::string("string");
			if (value.isBool())
				return std::string("bool");
			if (value.isList())
				return std::string("list");
			if (value.isMap())
				return std::string("map");

			return std::string("unknown");
		}
		virtual std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<native fn>"; }
	};
} // namespace Frontend
