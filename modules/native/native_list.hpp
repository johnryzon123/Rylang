#include <stdexcept>
#include "common.h"
#include "func.h"

namespace Frontend {
	struct RyLen : public RyCallable {
		int arity() override { return 1; }

		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override {
			RyValue arg = arguments[0];

			if (arg.isList()) {
				auto vec = arg.asList();
				return static_cast<double>(vec->size());
			}

			if (arg.isString()) {
				auto str = arg.asString();
				return static_cast<double>(str.length());
			}

			if (arg.isMap()) {
				auto env = arg.asMap();
				return (double) env->size();
			}

			throw std::runtime_error("Argument to len() must be a list, string, or map.");
		}
		virtual std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<native fn len>"; }
	};
	struct RyPop : public RyCallable {
		int arity() override { return 1; }

		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override {
			RyValue &arg = arguments[0];

			if (arg.isList()) {
				auto list = arg.asList();
				if (list->empty()) {
					throw std::runtime_error("Cannot pop from an empty list.");
				}
				RyValue lastElement = list->back();
				list->pop_back();
				return lastElement;
			}

			throw std::runtime_error("Argument to pop() must be a list.");
		}
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}

		std::string toString() override { return "<native fn pop>"; }
	};
} // namespace Frontend
