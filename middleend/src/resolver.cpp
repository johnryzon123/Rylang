#include "resolver.h"
#include "tools.h"
#include "runtime.h"

using namespace Backend;
using namespace RyTools;

void Resolver::resolve(const std::vector<std::shared_ptr<Stmt>> &statements) {
	for (const auto &statement: statements) {
		resolve(statement);
	}
}

void Resolver::resolve(std::shared_ptr<Stmt> stmt) { stmt->accept(*this); }

void Resolver::resolve(std::shared_ptr<Expr> expr) { expr->accept(*this); }

void Resolver::resolveFunction(std::shared_ptr<FunctionStmt> function, FunctionType type) {
	FunctionType enclosingFunction = currentFunction;
	currentFunction = type;

	beginScope();
	for (const auto &param: function->parameters) {
		declare(param.name);
		define(param.name);
	}
	resolve(function->body);
	endScope();

	currentFunction = enclosingFunction;
}

void Resolver::declare(Token name) {
	if (scopes.empty()) {
		globalSymbols[name.lexeme] = false;
		return;
	}

	auto &scope = scopes.back();
	if (scope.contains(name.lexeme)) {
		// Ry's for You doesn't allow re-declaring in the same local scope
		throw RyRuntimeError(name, "Already a variable with this name in this scope.");
	}

	scope[name.lexeme] = false; // "false" means declared but not yet defined
}

void Resolver::define(Token name) {
	if (scopes.empty()) {
		globalSymbols[name.lexeme] = true;
		return;
	}
	scopes.back()[name.lexeme] = true; // "true" means ready for use
}

void Resolver::visitVarStmt(VarStmt &stmt) {
	declare(stmt.name);
	if (stmt.initializer != nullptr) {
		resolve(stmt.initializer);
	}
	define(stmt.name);
}

void Resolver::visitFunctionStmt(FunctionStmt &stmt) {
	declare(stmt.name);
	define(stmt.name);

	beginScope();
	for (const auto &param: stmt.parameters) {
		declare(param.name);
		define(param.name);
	}
	// Resolve the statements inside the function body
	for (const auto &s: stmt.body) {
		resolve(s);
	}
	endScope();
}

void Resolver::resolveLocal(Expr *expr, Token name) {
	for (int i = scopes.size() - 1; i >= 0; i--) {
		if (scopes[i].contains(name.lexeme)) {
			// Tell the interpreter how many hops up the chain it is
			interpreter.resolve(expr, scopes.size() - 1 - i);
			return;
		}
	}
	if (globalSymbols.contains(name.lexeme)) {
		interpreter.resolve(expr, -1);
	}
}

void Resolver::visitIfStmt(IfStmt &stmt) {
	resolve(stmt.condition);
	resolve(stmt.thenBranch);
	if (stmt.elseBranch != nullptr)
		resolve(stmt.elseBranch);
}

void Resolver::visitWhileStmt(WhileStmt &stmt) {
	resolve(stmt.condition);
	resolve(stmt.body);
}

void Resolver::visitClassStmt(ClassStmt &stmt) {
	ClassType enclosingClass = currentClass;
	currentClass = ClassType::CLASS;

	declare(stmt.name);
	define(stmt.name);

	if (stmt.superclass != nullptr) {
		if (stmt.name.lexeme == stmt.superclass->name.lexeme) {
			throw RyRuntimeError(stmt.superclass->name, "A class cannot inherit from itself.");
		}
		resolve(stmt.superclass);
	}

	if (stmt.superclass != nullptr) {
		beginScope();
		scopes.back()["parent"] = true;
	}

	// Start a scope specifically for 'this'
	beginScope();
	Token thisToken = {TokenType::THIS, "this", RyValue(), (int) stmt.name.line, 0};
	declare(thisToken);
	define(thisToken);

	// Resolve methods
	for (auto &method: stmt.methods) {
		FunctionType declaration = FunctionType::METHOD;
		if (method->name.lexeme == "init") {
			declaration = FunctionType::INITIALIZER;
		}
		resolveFunction(method, declaration);
	}

	// Close the 'this' scope
	endScope();

	if (stmt.superclass != nullptr) {
		endScope();
	}

	currentClass = enclosingClass;
}

void Resolver::visitVariable(VariableExpr &expr) {
	if (!scopes.empty() && scopes.back().contains(expr.name.lexeme) && scopes.back()[expr.name.lexeme] == false) {
		throw RyRuntimeError(expr.name, "Can't read local variable in its own initializer.");
	}

	resolveLocal(&expr, expr.name);
}

void Resolver::visitAssign(AssignExpr &expr) {
	resolve(expr.value); // Resolve the value being assigned
	resolveLocal(&expr, expr.name); // Resolve the variable being assigned TO
}

void Resolver::visitMath(MathExpr &expr) {
	resolve(expr.left);
	resolve(expr.right);
}

void Resolver::visitCall(CallExpr &expr) {
	resolve(expr.callee);
	for (const auto &arg: expr.arguments) {
		resolve(arg);
	}
}

// Expressions that don't affect scope or contain sub-expressions to resolve
void Resolver::visitValue(ValueExpr &expr) {} // Literals like 5 or "hi" need no resolution

// Expressions that just need to resolve their children
void Resolver::visitGroup(GroupExpr &expr) { resolve(expr.expression); }
void Resolver::visitPrefix(PrefixExpr &expr) { resolve(expr.right); }
void Resolver::visitPostfix(PostfixExpr &expr) { resolve(expr.left); }
void Resolver::visitLogical(LogicalExpr &expr) {
	resolve(expr.left);
	resolve(expr.right);
}

void Resolver::visitThis(ThisExpr &expr) {
	if (currentClass == ClassType::NONE) {
		throw RyRuntimeError(expr.keyword, "Cannot use 'this' outside of a class.");
	}
	// 2. Resolve "this" as a local variable
	resolveLocal(&expr, expr.keyword);
}

// Complex types - we must resolve the items inside them
void Resolver::visitList(ListExpr &expr) {
	for (auto &element: expr.elements)
		resolve(element);
}
void Resolver::visitMap(MapExpr &expr) {
	for (auto &pair: expr.items) {
		resolve(pair.first);
		resolve(pair.second);
	}
}

// Control Flow Stmts
void Resolver::visitExpressionStmt(ExpressionStmt &stmt) { resolve(stmt.expression); }
void Resolver::visitStopStmt(StopStmt &stmt) {}
void Resolver::visitSkipStmt(SkipStmt &stmt) {}
void Resolver::visitImportStmt(ImportStmt &stmt) {}
void Resolver::visitAliasStmt(AliasStmt &stmt) {
	resolve(stmt.aliasExpr);
	declare(stmt.name);
	define(stmt.name);
}
void Resolver::visitNamespaceStmt(NamespaceStmt &stmt) {
	declare(stmt.name);
	define(stmt.name);
	resolve(stmt.body);
}
void Resolver::visitEachStmt(EachStmt &stmt) {
	resolve(stmt.collection);
	beginScope();
	declare(stmt.id);
	define(stmt.id);
	resolve(stmt.body);
	endScope();
}
void Resolver::visitBlockStmt(BlockStmt &stmt) {
	beginScope();
	resolve(stmt.statements);
	endScope();
}
void Resolver::visitReturnStmt(ReturnStmt &stmt) {
	if (stmt.value != nullptr)
		resolve(stmt.value);
}
void Resolver::visitForStmt(ForStmt &stmt) {
	beginScope();
	if (stmt.init != nullptr)
		resolve(stmt.init);
	if (stmt.condition != nullptr)
		resolve(stmt.condition);
	if (stmt.increment != nullptr)
		resolve(stmt.increment);
	resolve(stmt.body);
	endScope();
}
void Resolver::visitSet(SetExpr &expr) {
	resolve(expr.object);
	resolve(expr.value);
}
void Resolver::visitGet(GetExpr &expr) { resolve(expr.object); }
void Resolver::visitRange(RangeExpr &expr) {
	resolve(expr.leftBound);
	resolve(expr.rightBound);
}
void Resolver::visitIndexSet(IndexSetExpr &expr) {
	resolve(expr.object);
	resolve(expr.index);
	resolve(expr.value);
}
void Resolver::visitIndex(IndexExpr &expr) {
	resolve(expr.object);
	resolve(expr.index);
}
void Resolver::visitBitwiseOr(BitwiseOrExpr &expr) {
	resolve(expr.left);
	resolve(expr.right);
}
void Resolver::visitBitwiseXor(BitwiseXorExpr &expr) {
	resolve(expr.left);
	resolve(expr.right);
}
void Resolver::visitBitwiseAnd(BitwiseAndExpr &expr) {
	resolve(expr.left);
	resolve(expr.right);
}
void Resolver::visitShift(ShiftExpr &expr) {
	resolve(expr.left);
	resolve(expr.right);
}
void Resolver::visitAttemptStmt(AttemptStmt &stmt) {
	beginScope();
	resolve(stmt.attemptBody);
	endScope();

	beginScope();

	declare(stmt.error);
	define(stmt.error);

	resolve(stmt.failBody);
	endScope();

	beginScope();
	resolve(stmt.finallyBody);
	endScope();
}
void Resolver::visitPanicStmt(PanicStmt &stmt) {
	beginScope();
	if (stmt.message != nullptr)
		resolve(stmt.message);
	endScope();
}
