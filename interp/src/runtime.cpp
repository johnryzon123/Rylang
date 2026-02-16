//
// Created by ryzon on 12/29/25.
//
#include "../include/runtime.h"
#include <cctype>
#include <cmath>
#include <iostream>
#include <memory>
#include <ostream>
#include "env.h"
#include "lexer.h"
#include "native_io.hpp"
#include "native_list.hpp"
#include "native_sys.hpp"
#include "parser.h"
#include "resolver.h"
#include "tools.h"

using namespace RyTools;

namespace Frontend {
	// Inside namespace Frontend in runtime.cpp
	void reset(Interpreter &interp) {
		RyTools::hadError = false;
		interp.typeAliases.clear();
	}
	bool Interpreter::tryToDouble(const RyValue &v, double &out) {
		try {
			if (v.isNumber()) {
				out = v.asNumber();
				return true;
			}
			if (v.isString()) {
				std::string s = v.asString();
				// trim
				auto f = s.find_first_not_of(" \t\r\n");
				if (f == std::string::npos)
					return false;
				auto l = s.find_last_not_of(" \t\r\n");
				s = s.substr(f, l - f + 1);
				size_t idx = 0;
				double val = std::stod(s, &idx);
				if (idx == s.size()) {
					out = val;
					return true;
				}
				return false;
			}
		} catch (...) {
		}
		return false;
	};
	/**
	 * Checks if the given value is truthy.
	 *
	 * A value is considered truthy if it is:
	 *   - a boolean with value true
	 *   - a non-null pointer
	 *   - a non-empty string
	 *   - a non-zero number
	 *   - any other non-empty object
	 *
	 * @param value the value to check
	 * @return true if the value is truthy, false otherwise
	 */
	bool Interpreter::isTruthy(const RyValue &value) {
		if (value.isNil())
			return false;
		if (value.isBool())
			return value.asBool();
		return true;
	}
	void Interpreter::checkType(const Token &name, const std::string &constraint, const RyValue &value) {
		if (constraint.empty())
			return;

		if (constraint == "string") {
			if (value.isString())
				return;
			else if (value.isNumber()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a string but got a number.");
			} else if (value.isBool()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a string but got a boolean.");
			} else if (value.isList()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a string but got a list.");
			} else if (value.isMap()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a string but got a map.");
			} else {
				throw RyRuntimeError(name, "Type Error: Variable expects a string but got an unexpected type");
			}
			return;
		} else if (constraint == "num") {
			if (value.isNumber())
				return;
			else if (value.isString()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a number but got a string");
			} else if (value.isBool()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a number but got a boolean.");
			} else if (value.isList()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a number but got a list.");
			} else if (value.isMap()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a number but got a map.");
			} else {
				throw RyRuntimeError(name, "Type Error: Variable expects a number but got an unexpected type");
			}
			return;
		} else if (constraint == "bool") {
			if (value.isBool())
				return;
			else if (value.isString()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a boolean but got a string");
			} else if (value.isNumber()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a boolean but got a number.");
			} else if (value.isList()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a boolean but got a list.");
			} else if (value.isMap()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a boolean but got a map.");
			} else {
				throw RyRuntimeError(name, "Type Error: Variable expects a boolean but got an unexpected type");
			}
			return;
		} else if (constraint == "list") {
			if (value.isList())
				return;
			else if (value.isString()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a list but got a string");
			} else if (value.isBool()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a list but got a boolean.");
			} else if (value.isNumber()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a list but got a number.");
			} else if (value.isMap()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a list but got a map.");
			} else {
				throw RyRuntimeError(name, "Type Error: Variable expects a list but got an unexpected type");
			}
			return;
		} else if (constraint == "map") {
			if (value.isMap())
				return;
			else if (value.isString()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a map but got a string");
			} else if (value.isBool()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a map but got a boolean.");
			} else if (value.isList()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a map but got a list.");
			} else if (value.isNumber()) {
				throw RyRuntimeError(name, "Type Error: Variable expects a map but got a number.");
			} else {
				throw RyRuntimeError(name, "Type Error: Variable expects a map but got an unexpected type");
			}
			return;
		} else {
			throw RyRuntimeError(name, "Type Error: Unexpected type.");
		}
	}
	std::string Interpreter::resolveType(std::optional<Token> prefix, Token alias) {
		if (prefix.has_value()) {
			if (environment->has(prefix->lexeme, *prefix)) {
				auto obj = environment->get(prefix->lexeme, *prefix);
				if (obj.isMap()) {
					auto ns = obj.asMap();
					if (ns->isTypeAlias(alias.lexeme)) {
						return ns->getTypeAlias(alias.lexeme);
					}
				}
			}
		} else {
			return getAliasTarget(alias.lexeme);
		}
		return "";
	}
	std::string Interpreter::getAliasTarget(const std::string &name) {
		if (environment->isTypeAlias(name)) {
			return environment->getTypeAlias(name);
		}
		return ""; // Not an alias, no constraint
	}
	bool Interpreter::areEqual(const RyValue &a, const RyValue &b) {
		// Nil handling: Two nils are equal.
		if (a.isNil() && b.isNil())
			return true;
		if (a.isNil() || b.isNil())
			return false;

		// If they aren't the same type, they aren't equal.
		if (a.val.index() != b.val.index())
			return false;

		// Content Check: Based on the type
		if (a.isBool())
			return a.asBool() == b.asBool();
		if (a.isNumber())
			return a.asNumber() == b.asNumber();
		if (a.isString())
			return a.asString() == b.asString();

		// List Equality
		if (a.isList()) {
			auto listA = a.asList();
			auto listB = b.asList();

			if (listA->size() != listB->size())
				return false;

			for (size_t i = 0; i < listA->size(); ++i) {
				// Recursive call to check every element
				if (!areEqual((*listA)[i], (*listB)[i]))
					return false;
			}
			return true;
		}

		// 5. Map/Environment Equality
		if (a.isMap()) {
			// Usually, maps are compared by reference or you can
			// implement a key-by-key comparison similar to the list.
			return a.asMap() == b.asMap();
		}

		return false;
	}
	bool Interpreter::isInternalAccess(std::shared_ptr<RyInstance> instance) {
		try {
			RyValue val = environment->get(Token(TokenType::THIS, "this", nullptr, 0, 0));

			if (val.isInstance()) {
				return val.asInstance() == instance;
			}
		} catch (...) {
			return false;
		}
		return false;
	}
	std::shared_ptr<Environment> &Interpreter::getGlobals() { return globals; }
	void Interpreter::defineNative(std::string name, std::shared_ptr<RyCallable> callable) {
		globals->define(name, callable);
	}

	/**
	 * Execute a block of statements in a local environment.
	 *
	 * @param statements List of statements to execute
	 * @param localEnv Environment in which to execute the statements
	 */
	void Interpreter::executeBlock(const std::vector<std::shared_ptr<Stmt>> &statements, Environment &localEnv) {
		std::shared_ptr<Environment> previous = this->environment;

		try {
			this->environment = std::shared_ptr<Backend::Environment>(&localEnv, [](auto *) {});

			for (const auto &stmt: statements) {
				execute(stmt.get());
			}
		} catch (...) {
			this->environment = previous;
			throw;
		}

		this->environment = previous;
	}

	/// Interprets a list of statements and executes them in order.
	/// Statements are executed by calling execute() on each statement.
	/// The execute() function is responsible for evaluating the statement and
	/// performing any necessary side effects.
	void Interpreter::interpret(const std::vector<std::shared_ptr<Stmt>> &statements) {
		for (const auto &stmt: statements) {
			if (stmt == nullptr)
				continue;
			execute(stmt.get());
		}
	}
	bool Interpreter::isTypeAlias(std::shared_ptr<Expr> expr) {
		// If the expression is just a variable name like 'bool' or 'string'
		// but it came from a 'data::' alias, it will be a VariableExpr.
		return dynamic_cast<VariableExpr *>(expr.get()) != nullptr;
	}

	std::string Interpreter::getTypeName(std::shared_ptr<Expr> expr) {
		if (auto var = dynamic_cast<VariableExpr *>(expr.get())) {
			return var->name.lexeme;
		}
		return "";
	}
	/// Runs the given source code in the interpreter and reports any errors.
	/// @param src The source code to run
	/// @param interp The interpreter to use
	/// @throws RyTools::RyRuntimeError If there are any errors in the source code, throws a RyTools::RyRuntimeError with
	/// the error information.
	void run(const std::string &src, Interpreter &interp, Resolver &resolver) {
		RyTools::hadError = false;
		Backend::Lexer lexer(src);
		lexer.scanTokens();
		const auto &tokens = lexer.getTokens();

		Backend::Parser parser(tokens, interp.typeAliases, src);
		auto statements = parser.parse();

		if (!RyTools::hadError) {
			try {
				resolver.resolve(statements);
				interp.interpret(statements);
			} catch (const RyTools::RyRuntimeError &error) {
				std::string errorLine = "";
				std::stringstream ss(src);
				std::string temp;
				size_t currentLine = 1;

				// Find the specific line text
				while (std::getline(ss, temp)) {
					if (currentLine == error.token.line) {
						errorLine = temp;
						break;
					}
					currentLine++;
				}

				// Call report with the new sourceLine argument

				RyTools::report(error.token.line, error.token.column, "", error.message, src, !error.isPanicking);
				RyTools::hadError = true;
			} catch (const std::exception &e) {
				std::cerr << "Internal System Error: " << e.what() << std::endl;
			}
		}
	}

	extern "C" {
	void *create_interpreter() { return static_cast<void *>(new Interpreter()); }
	void *create_resolver(void *interp) {
		if (interp == nullptr)
			return nullptr;
		auto *interpreterPtr = static_cast<Interpreter *>(interp);
		return static_cast<void *>(new Resolver(*interpreterPtr));
	}
	void destroy_interpreter(void *interp) {
		if (interp) {
			delete static_cast<Interpreter *>(interp);
			interp = nullptr; // Prevent double-deletion
		}
	}
	void destroy_resolver(void *resolver) {
		if (resolver) {
			delete static_cast<Resolver *>(resolver);
			resolver = nullptr;
		}
	}
	void reset_interpreter(void *interp, void *resolver) {
		if (interp == nullptr || resolver == nullptr)
			return;
		reset(*static_cast<Interpreter *>(interp));
	}

	/// Runs the given source code in the interpreter and reports any errors.
	///
	/// @param interp The interpreter to use
	/// @param src The source code to run
	///
	/// If the source code contains any errors, throws a RyTools::RyRuntimeError with the error information.
	void run_source(void *interp, void *resolver, const char *src) {
		if (interp == nullptr || src == nullptr || resolver == nullptr)
			return;
		run(std::string(src), *static_cast<Interpreter *>(interp), *static_cast<Resolver *>(resolver));
	}
	}
} // namespace Frontend
