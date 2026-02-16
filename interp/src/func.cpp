#include <memory>
#include "common.h"
#include "runTools.h"
#include "runtime.h"


namespace Frontend {
	/// Call a RyFunction with the given arguments.
	///
	/// Creates a new environment for this function invocation,
	/// defines the parameters in the local environment,
	/// and executes the function body.
	///
	/// If the function returns (either explicitly or implicitly),
	/// the return value is propagated to the caller.
	RyValue RyFunction::call(Interpreter &interpreter, std::vector<RyValue> arguments) {
		// Create a new environment for the function call (local scope)
		auto environment = std::make_shared<Backend::Environment>(closure);

		std::shared_ptr<Environment> previous = interpreter.environment;
		interpreter.environment = environment;

		try {
			for (int i = 0; i < declaration->parameters.size(); i++) {
				const auto &param = declaration->parameters[i];

				if (i < arguments.size()) {
					environment->define(param.name.lexeme, arguments[i]);
				} else if (param.defaultValue != nullptr) {
					param.defaultValue->accept(interpreter);

					RyValue value = interpreter.lastResult;

					environment->define(param.name.lexeme, value);
				}
			}
		} catch (...) {
			interpreter.environment = previous;
			throw;
		}

		RyValue result = nullptr;
		// Execute the function bod
		try {
			interpreter.executeBlock(declaration->body, *environment);
		} catch (ReturnSignal &returnValue) {
			interpreter.environment = previous;
			result = returnValue.value;
		} catch (...) {
			interpreter.environment = previous;
			throw;
		}
		interpreter.environment = previous;


		if (declaration->returnTypeAlias.has_value()) {
			std::shared_ptr<Environment> callerEnv = std::make_shared<Environment>(interpreter.environment);
			interpreter.environment = closure;
			std::string constraint;
			try {
				constraint = interpreter.resolveType(declaration->returnTypeNamespace, declaration->returnTypeAlias.value());
			} catch (...) {
				interpreter.environment = callerEnv;
				throw;
			}
			interpreter.environment = callerEnv;

			if (!constraint.empty()) {
				interpreter.checkType(declaration->name, constraint, result);
			}
		}
		if (result.isNumber()) {
			return RyValue(result.asNumber());
		} else if (result.isBool()) {
			return RyValue(result.asBool());
		} else if (result.isString()) {
			return RyValue(result.asString());
		} else if (result.isList()) {
			return RyValue(result.asList());
		} else if (result.isMap()) {
			return RyValue(result.asMap());
		} else if (result.isFunction()) {
			return RyValue(result.asFunction());
		}
		if (isInitializer) {
			// Look up "this" in the environment that just used
			return closure->getAt(0, "this").value;
		}
		return result;
	}

	std::shared_ptr<RyFunction> RyFunction::bind(std::shared_ptr<RyInstance> instance) {
		// Create a new environment wrapped around the original closure
		auto environment = std::make_shared<Backend::Environment>(closure);

		// Define "this" as the specific instance
		environment->define("this", instance);
		bool isInit = (declaration->name.lexeme == "init");
		return std::make_shared<RyFunction>(declaration, environment, isPrivate, isInitializer);
	}

	std::string RyFunction::toString() { return "<fn " + declaration->name.lexeme + ">"; }
} // namespace Frontend
