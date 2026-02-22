#include "lexer.h"
#include "parser.h"
#include "runtime.h"

namespace Frontend {
	void Interpreter::execute(Stmt *stmt) { stmt->accept(*this); }
	/// Interprets the given source code and executes it.
	///
	/// If the source code is a module, this function will not crash the whole engine if the module contains a syntax
	/// error.
	void Interpreter::interpretSource(const std::string &source) {
		Lexer scanner(source);
		std::vector<Token> tokens = scanner.scanTokens();

		Parser parser(tokens, typeAliases, source);
		std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

		// Don't let a syntax error in a module crash the whole engine
		if (statements.empty())
			return;

		this->interpret(statements); // This calls your existing execution logic
	}
	/// Evaluates the given expression and returns the result.
	///
	/// This function will call the accept function on the given expression
	/// and return the result of the evaluation.
	///
	/// @return The result of the evaluation.
	RyValue Interpreter::evaluate(Expr *expr) {
		expr->accept(*this);
		return lastResult;
	}

} // namespace Frontend
