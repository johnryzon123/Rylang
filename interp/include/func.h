#pragma once
#include "common.h"
#include "env.h"
#include "stmt.h"

namespace Frontend {
	class RyCallable {
	public:
		virtual ~RyCallable() = default;
		virtual int arity() = 0; // Number of arguments expected
		virtual RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) = 0;
		virtual std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) = 0;
		virtual std::string toString() = 0;
	};
	class RyFunction : public RyCallable {
		friend class Interpreter;

	private:
		std::shared_ptr<Backend::FunctionStmt> declaration; // The AST node from the parser
		std::shared_ptr<Backend::Environment> closure;
		bool isPrivate = false;
		bool isInitializer = false;

	public:
		RyFunction(std::shared_ptr<Backend::FunctionStmt> declaration, std::shared_ptr<Backend::Environment> closure,
							 bool isPrivate = false, bool isInitializer = false) :
				declaration(std::move(declaration)), closure(closure), isPrivate(isPrivate), isInitializer(isInitializer) {}

		// Logic: Return how many parameters the user defined in Ry
		int arity() override {
			int required = 0;
			for (const auto &param: declaration->parameters) {
				if (param.defaultValue == nullptr)
					required++;
			}
			return required;
		}

		RyValue call(Interpreter &interpreter, std::vector<RyValue> arguments) override;
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override;
		bool getPrivate() { return isPrivate; }
		std::string toString() override;
	};
} // namespace Frontend
