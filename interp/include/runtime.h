//
// Created by ryzon on 12/29/25.
//

#pragma once
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "common.h"
#include "env.h"
#include "expr.h"
#include "func.h"
#include "stmt.h"

using namespace Backend;

namespace Frontend {
	struct StopSignal {};
	struct SkipSignal {};
	class Interpreter : public ExprVisitor, public StmtVisitor {
		friend class RyFunction;
		friend class RyClass;

	private:
		void execute(Stmt *stmt);
		std::shared_ptr<Environment> environment;
		std::shared_ptr<Environment> globals;
		RyValue lastResult;
		bool areEqual(const RyValue &a, const RyValue &b);
		bool isTypeAlias(std::shared_ptr<Expr> expr);
		std::string getTypeName(std::shared_ptr<Expr> expr);
		RyValue evaluate(Expr *expr);
		static bool isTruthy(const RyValue &value);
		std::string getAliasTarget(const std::string &name);
		bool tryToDouble(const RyValue &v, double &out);
		bool isInternalAccess(std::shared_ptr<RyInstance> instance);
		std::map<Backend::Expr *, int> locals;
		bool is_panicking = false;
		RyValue panicMessage;

	public:
		std::set<std::string> typeAliases;
		std::set<std::string> loaded_modules;
		Interpreter();
		void resolve(Backend::Expr *expr, int depth) { locals[expr] = depth; }
		void checkType(const Token &name, const std::string &constraint, const RyValue &value);
		std::string resolveType(std::optional<Token> prefix, Token alias);
		std::shared_ptr<Environment> &getGlobals();
		void defineNative(std::string name, std::shared_ptr<RyCallable> callable);
		void interpretSource(const std::string &source);
		void executeBlock(const std::vector<std::shared_ptr<Stmt>> &statements, Environment &localEnv);
		void interpret(const std::vector<std::shared_ptr<Stmt>> &statements);
		void visitValue(ValueExpr &expr) override;
		void visitMath(MathExpr &expr) override;
		void visitBitwiseOr(BitwiseOrExpr &expr) override;
		void visitBitwiseXor(BitwiseXorExpr &expr) override;
		void visitBitwiseAnd(BitwiseAndExpr &expr) override;
		void visitPrefix(PrefixExpr &expr) override;
		void visitPostfix(PostfixExpr &expr) override;
		void visitShift(ShiftExpr &expr) override;
		void visitGroup(GroupExpr &expr) override;
		void visitVariable(VariableExpr &expr) override;
		void visitLogical(LogicalExpr &expr) override;
		void visitAssign(AssignExpr &expr) override;
		void visitCall(CallExpr &expr) override;
		void visitThis(ThisExpr &expr) override;
		void visitGet(GetExpr &expr) override;
		void visitMap(MapExpr &expr) override;
		void visitRange(RangeExpr &expr) override;
		void visitSet(SetExpr &expr) override;
		void visitIndexSet(IndexSetExpr &expr) override;
		void visitBlockStmt(BlockStmt &stmt) override;
		void visitExpressionStmt(ExpressionStmt &stmt) override;
		void visitFunctionStmt(FunctionStmt &stmt) override;
		void visitImportStmt(ImportStmt &stmt) override;
		void visitAliasStmt(AliasStmt &stmt) override;
		void visitVarStmt(VarStmt &stmt) override;
		void visitReturnStmt(ReturnStmt &stmt) override;
		void visitWhileStmt(WhileStmt &stmt) override;
		void visitIfStmt(IfStmt &stmt) override;
		void visitList(ListExpr &expr) override;
		void visitIndex(IndexExpr &expr) override;
		void visitNamespaceStmt(NamespaceStmt &stmt) override;
		void visitEachStmt(EachStmt &stmt) override;
		void visitStopStmt(StopStmt &stmt) override;
		void visitSkipStmt(SkipStmt &stmt) override;
		void visitForStmt(ForStmt &stmt) override;
		void visitClassStmt(ClassStmt &stmt) override;
		void visitAttemptStmt(AttemptStmt &stmt) override;
		void visitPanicStmt(PanicStmt &stmt) override;
	};

} // namespace Frontend

// C API for embedding the interpreter from other languages / shared libraries
extern "C" {
void *create_interpreter();
void *create_resolver(void *interp);
void destroy_interpreter(void *interp);
void destroy_resolver(void *resolver);
void reset_interpreter(void *interp, void *resolver);
void run_source(void *interp, void *resolver, const char *src);
}
