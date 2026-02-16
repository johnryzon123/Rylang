#include <map>
#include <string>
#include <vector>
#include "common.h"
#include "expr.h"
#include "stmt.h"

using namespace Backend;


class Resolver : public Backend::ExprVisitor, public Backend::StmtVisitor {
	Frontend::Interpreter &interpreter;
	std::vector<std::map<std::string, bool>> scopes; // Stack of scopes
	std::map<std::string, int> globalSymbols;
	enum class FunctionType { NONE, FUNCTION, METHOD, INITIALIZER };

	// This tells us if we are inside a class definition
	enum class ClassType { NONE, CLASS };

	FunctionType currentFunction = FunctionType::NONE;
	ClassType currentClass = ClassType::NONE;

public:
	Resolver(Frontend::Interpreter &interpreter) : interpreter(interpreter) {}

	// Helper to start a new scope level
	void beginScope() { scopes.push_back(std::map<std::string, bool>()); }
	void endScope() { scopes.pop_back(); }
	void declare(Token token);
	void define(Token token);
	void resolve(std::shared_ptr<Expr> expr);
	void resolve(std::shared_ptr<Stmt> stmt);
	void resolve(const std::vector<std::shared_ptr<Stmt>> &statements);
	void resolveLocal(Expr *expr, Token name);
	void resolveFunction(std::shared_ptr<FunctionStmt> function, FunctionType type);

	// visitors
	void visitVarStmt(VarStmt &stmt);
	void visitFunctionStmt(FunctionStmt &stmt);
	void visitIfStmt(IfStmt &stmt);
	void visitWhileStmt(WhileStmt &stmt);
	void visitVariable(VariableExpr &expr);
	void visitAssign(AssignExpr &expr);
	void visitMath(MathExpr &expr);
	void visitCall(CallExpr &expr);
	void visitThis(ThisExpr &expr);
	void visitValue(ValueExpr &expr);
	void visitGroup(GroupExpr &expr);
	void visitPrefix(PrefixExpr &expr);
	void visitPostfix(PostfixExpr &expr);
	void visitLogical(LogicalExpr &expr);
	void visitList(ListExpr &expr);
	void visitMap(MapExpr &expr);
	void visitExpressionStmt(ExpressionStmt &stmt);
	void visitStopStmt(StopStmt &stmt);
	void visitSkipStmt(SkipStmt &stmt);
	void visitImportStmt(ImportStmt &stmt);
	void visitAliasStmt(AliasStmt &stmt);
	void visitNamespaceStmt(NamespaceStmt &stmt);
	void visitEachStmt(EachStmt &stmt);
	void visitBlockStmt(BlockStmt &stmt);
	void visitReturnStmt(ReturnStmt &stmt);
	void visitForStmt(ForStmt &stmt);
	void visitClassStmt(ClassStmt &stmt);
	void visitSet(SetExpr &expr);
	void visitGet(GetExpr &expr);
	void visitRange(RangeExpr &expr);
	void visitIndexSet(IndexSetExpr &expr);
	void visitIndex(IndexExpr &expr);
	void visitBitwiseOr(BitwiseOrExpr &expr);
	void visitBitwiseXor(BitwiseXorExpr &expr);
	void visitBitwiseAnd(BitwiseAndExpr &expr);
	void visitShift(ShiftExpr &expr);
	void visitAttemptStmt(AttemptStmt &stmt);
	void visitPanicStmt(PanicStmt &stmt);
};
