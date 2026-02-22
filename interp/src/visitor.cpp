#include <cmath>
#include "class.h"
#include "common.h"
#include "env.h"
#include "lexer.h"
#include "native_io.hpp"
#include "native_list.hpp"
#include "native_sys.hpp"
#include "parser.h"
#include "resolver.h"
#include "runTools.h"
#include "runtime.h"
#include "stmt.h"
#include "tools.h"

using namespace RyTools;

namespace Frontend {
	/// Visits a ValueExpr and evaluates it to a value.
	/// If the value is a number, it is converted to a double.
	/// If the value is a string, it is trimmed of its quotes and returned as a std::string.
	/// If the value is a boolean, it is returned as a bool.
	/// If the value is unknown, nullptr is returned.
	void Interpreter::visitValue(ValueExpr &expr) {
		if (is_panicking)
			return;
		if (expr.value.type == TokenType::NUMBER) {
			lastResult = std::stod(expr.value.lexeme);
		} else if (expr.value.type == TokenType::STRING) {
			if (std::string s = expr.value.lexeme; s.length() >= 2 && s.front() == '"' && s.back() == '"') {
				lastResult = s.substr(1, s.length() - 2);
			} else {
				lastResult = s;
			}
		} else if (expr.value.type == TokenType::TRUE) {
			lastResult = true;
		} else if (expr.value.type == TokenType::FALSE) {
			lastResult = false;
		} else {
			lastResult = RyValue();
		}
	}

	void Interpreter::visitSet(SetExpr &expr) {
		if (is_panicking)
			return;
		RyValue object = evaluate(expr.object.get());
		RyValue value = evaluate(expr.value.get());

		if (object.isInstance()) {
			auto instance = object.asInstance();
			RyVariable var = instance->getVariable(expr.name);

			// Privacy Check
			if (var.isPrivate && !isInternalAccess(instance)) {
				throw RyRuntimeError(expr.name, "Cannot access private member '" + expr.name.lexeme + "'.");
			}

			// Type Constraint Check
			if (var.typeConstraint.has_value()) {
				std::string constraint = var.typeConstraint.value();
				checkType(expr.name, constraint, value);
			}


			var.value = value;
			instance->set(expr.name, var);
			lastResult = value; // Set lastResult for chained assignments!
			return;
		}
		if (object.isMap()) {
			RyValue value = evaluate(expr.value.get());
			auto env = object.asMap();
			env->define(expr.name.lexeme, value);
			lastResult = value;
			return;
		}

		throw RyRuntimeError(expr.name, "Only modules and objects have properties.");
	}

	void Interpreter::visitMap(MapExpr &expr) {
		if (is_panicking)
			return;
		std::shared_ptr<Backend::Environment> map = std::make_shared<Backend::Environment>();

		for (const auto &pair: expr.items) {
			RyValue key = evaluate(pair.first.get());
			RyValue value = evaluate(pair.second.get());
			if (!key.isString()) {
				throw RyRuntimeError(expr.braceToken, "Map keys must be strings.");
			}
			map->define(key.asString(), value);
		}
		lastResult = map;
	}

	/// Visit a GetExpr node in the AST.
	///
	/// This node is used to get properties from modules and objects.
	///
	/// @param expr The GetExpr node to visit.
	///
	/// @return The property value if the property exists, or an RyRuntimeError if it does not.
	void Interpreter::visitGet(GetExpr &expr) {
		if (is_panicking)
			return;
		RyValue object = evaluate(expr.object.get());

		if (object.isMap()) {
			auto env = object.asMap();

			if (env->has(expr.name.lexeme, expr.name)) {
				lastResult = env->get(expr.name.lexeme, expr.name);
				return;
			}

			if (env->isTypeAlias(expr.name.lexeme)) {
				lastResult = env->getTypeAlias(expr.name.lexeme);
				return;
			}
		}
		if (object.isInstance()) {
			auto instance = object.asInstance();
			RyVariable var = instance->getVariable(expr.name); // Get the whole wrapper

			if (var.isPrivate && !isInternalAccess(instance)) {
				throw RyRuntimeError(expr.name, "Cannot access private member '" + expr.name.lexeme + "'.");
			}

			lastResult = var.value;
			return;
		}
		if (object.isFunction()) {
			auto klass = std::dynamic_pointer_cast<RyClass>(object.asFunction());
			if (klass) {
				auto method = klass->findMethod(expr.name.lexeme);
				if (method) {
					// Auto-bind logic for 'parent.method()' calls.
					// If we are accessing a method on a class (e.g. Animal.init),
					// and we are currently inside an instance of a subclass (e.g. Dog),
					// we want to bind 'this' to the current instance.
					Token thisToken = {TokenType::THIS, "this", RyValue(), 0, 0};
					if (environment->has("this", thisToken)) {
						RyValue thisVal = environment->get(thisToken);
						if (thisVal.isInstance()) {
							auto instance = thisVal.asInstance();
							// Check if instance's class inherits from 'klass'
							auto k = instance->getClass();
							while (k) {
								if (k == klass) {
									lastResult = std::static_pointer_cast<RyCallable>(method->bind(instance));
									return;
								}
								k = k->superclass;
							}
						}
					}

					lastResult = std::static_pointer_cast<RyCallable>(method);
					return;
				}
			}
		}
		throw RyRuntimeError(expr.name, "Undefined property.");
	}

	/**
	 * Visits a MathExpr node in the AST.
	 *
	 * This node is used to perform arithmetic and comparison operations.
	 *
	 * @param expr The MathExpr node to visit.
	 *
	 * @return The result of the operation.
	 *
	 * @throws RyRuntimeError If the operands are not numbers or matching types.
	 */
	void Interpreter::visitMath(MathExpr &expr) {
		if (is_panicking)
			return;
		RyValue left = evaluate(expr.left.get());
		RyValue right = evaluate(expr.right.get());

		const TokenType op = expr.op_t.type;
		if (op == TokenType::PLUS) {
			if (left.isList()) {
				auto oldList = left.asList();
				auto newList = std::make_shared<std::vector<RyValue>>(*oldList);
				if (right.isList()) {
					auto other = right.asList();
					newList->insert(newList->end(), other->begin(), other->end());
				} else {
					newList->push_back(right);
				}
				lastResult = newList;
				return;
			} else if (left.isString() || right.isString()) {
				lastResult = RyValue(left.toString() + right.toString());
				return;
			}
		}

		if (op == TokenType::MINUS && left.isList()) {
			auto oldList = left.asList();
			auto newList = std::make_shared<std::vector<RyValue>>();
			auto rightList = right.asList();


			// check if right is a list
			if (right.isList()) {
				for (const auto &item: *oldList) {
					bool found = false;
					for (const auto &other: *rightList) {
						if (areEqual(item, other)) {
							found = true;
							break;
						}
					}
					if (!found) {
						newList->push_back(item);
					}
				}
			} else {
				for (const auto &item: *oldList) {
					if (!areEqual(item, right)) {
						newList->push_back(item);
					}
				}
			}

			lastResult = newList;
			return;
		}

		double ld, rd;
		if (tryToDouble(left, ld) && tryToDouble(right, rd)) {
			switch (op) {
				case TokenType::PLUS:
					lastResult = ld + rd;
					return;
				case TokenType::MINUS:
					lastResult = ld - rd;
					return;
				case TokenType::STAR:
					lastResult = ld * rd;
					return;
				case TokenType::DIVIDE:
					if (rd == 0)
						throw RyRuntimeError(expr.op_t, "Cannot Divide with zero.", "MathError");
					lastResult = ld / rd;
					return;
				case TokenType::PERCENT:
					if (rd == 0)
						throw RyRuntimeError(expr.op_t, "Cannot get remainder of division with zero.", "MathError");

					// For double-based modulo (allows 10.5 % 3)
					lastResult = std::fmod(ld, rd);
					return;
				case TokenType::GREATER:
					lastResult = ld > rd;
					return;
				case TokenType::GREATER_EQUAL:
					lastResult = ld >= rd;
					return;
				case TokenType::LESS:
					lastResult = ld < rd;
					return;
				case TokenType::LESS_EQUAL:
					lastResult = ld <= rd;
					return;
				case TokenType::BANG_EQUAL:
					lastResult = ld != rd;
					return;
				case TokenType::EQUAL_EQUAL:
					lastResult = ld == rd;
					return;
				default:
					break;
			}
		}
		if (op == TokenType::EQUAL_EQUAL) {
			lastResult = areEqual(left, right);
			return;
		}

		if (op == TokenType::STAR) {
			// String Repeating: "Hi" * 3
			if (left.isString() && right.isNumber()) {
				std::string str = left.asString();
				int count = static_cast<int>(right.asNumber());

				std::string result = "";
				result.reserve(str.size() * count);
				for (int i = 0; i < count; i++)
					result += str;

				lastResult = result;
				return;
			}

			// Array Repeating: [1, 2] * 3
			if (left.isList() && right.isNumber()) {
				auto oldList = left.asList();
				int count = static_cast<int>(right.asNumber());

				auto newList = std::make_shared<std::vector<RyValue>>();
				for (int i = 0; i < count; i++) {
					newList->insert(newList->end(), oldList->begin(), oldList->end());
				}

				lastResult = newList;
				return;
			}
		}

		throw RyRuntimeError(expr.op_t, "Operands must be numbers or matching types.", "MathError");
	}

	void Interpreter::visitRange(RangeExpr &expr) {
		if (is_panicking)
			return;
		RyValue start = evaluate(expr.leftBound.get());
		RyValue end = evaluate(expr.rightBound.get());

		double ld, rd;
		// Use your tryToDouble helper here for cleaner code!
		if (!tryToDouble(start, ld) || !tryToDouble(end, rd)) {
			throw RyRuntimeError(expr.op_t, "Range bounds must be numbers.");
		}

		auto list = std::make_shared<std::vector<RyValue>>();

		if (ld <= rd) {
			// Counting Up
			for (double i = ld; i <= rd; i++) {
				list->push_back(i);
			}
		} else {
			// Counting Down
			for (double i = ld; i >= rd; i--) {
				list->push_back(i);
			}
		}

		lastResult = list;
	}

	/// Visits a PrefixExpr node in the AST.
	///
	/// This node is used to implement the prefix operator `-`.
	/// It negates the value of the expression to its right.
	///
	/// @param expr The node to visit.
	///
	/// @return The result of the expression.
	void Interpreter::visitPrefix(PrefixExpr &expr) {
		if (is_panicking)
			return;
		const RyValue right = evaluate(expr.right.get());
		if (expr.prefix.type == TokenType::PLUS_PLUS || expr.prefix.type == TokenType::MINUS_MINUS) {
			auto var = dynamic_cast<VariableExpr *>(expr.right.get());
			if (!var)
				throw RyRuntimeError(expr.prefix, "Target must be a variable.");

			// Get current value
			double value = environment->get(var->name).asNumber();

			// Calculate new value
			if (expr.prefix.type == TokenType::PLUS_PLUS)
				value++;
			else
				value--;

			// Update Environment
			environment->assign(var->name, RyVariable(value));

			// Return the NEW value
			lastResult = value;
			return;
		} else if (expr.prefix.type == TokenType::MINUS) {
			lastResult = -right.asNumber();
		} else if (expr.prefix.type == TokenType::BANG) {
			lastResult = !isTruthy(right);
		} else if (expr.prefix.type == TokenType::TILDE) {
			lastResult = (double) (~(long) evaluate(expr.right.get()).asNumber());
		}
	}

	void Interpreter::visitPostfix(PostfixExpr &expr) {
		if (is_panicking)
			return;
		auto var = dynamic_cast<VariableExpr *>(expr.left.get());
		if (!var)
			throw RyRuntimeError(expr.postfix, "Target must be a variable.");

		// Get the current (old) value
		double oldValue = environment->get(var->name).asNumber();

		// Calculate new value
		double newValue = (expr.postfix.type == TokenType::PLUS_PLUS) ? oldValue + 1 : oldValue - 1;

		// Update Environment with the NEW value
		environment->assign(var->name, RyVariable(newValue));

		// Return the OLD value
		lastResult = oldValue;
	}

	void Interpreter::visitShift(ShiftExpr &expr) {
		if (is_panicking)
			return;
		auto left = evaluate(expr.left.get());
		auto right = evaluate(expr.right.get());
		auto op = expr.op_t.type;

		if (op == TokenType::LESS_LESS) {
			if (!left.isNumber() || !right.isNumber()) {

				throw RyRuntimeError(expr.op_t, "Operands must be a number.");
			} else {
				// Convert to integers safely
				long l_val = static_cast<long>(left.asNumber());
				long r_val = static_cast<long>(right.asNumber());

				// Protect the engine from bad Ry code
				if (r_val < 0 || r_val >= 64) {
					lastResult = 0.0; // Or throw a RyRuntimeError
				} else {
					// Perform the shift and cast back to double for RyValue
					lastResult = static_cast<double>(l_val << r_val);
				}
			}
		} else if (op == TokenType::GREATER_GREATER) {
			if (!left.isNumber() || !right.isNumber()) {
				throw RyRuntimeError(expr.op_t, "Operands must be a number.");
			} else {
				// Convert to integers safely
				long l_val = static_cast<long>(left.asNumber());
				long r_val = static_cast<long>(right.asNumber());

				// Protect the engine from bad Ry code
				if (r_val < 0 || r_val >= 64) {
					lastResult = 0.0; // Or throw a RyRuntimeError
				} else {
					// Perform the shift and cast back to double for RyValue
					lastResult = static_cast<double>(l_val << r_val);
				}
			}
		} else {
			throw RyRuntimeError(expr.op_t, "Invalid shift operator.");
		}
	}


	void Interpreter::visitGroup(GroupExpr &expr) {
		if (is_panicking)
			return;
		lastResult = evaluate(expr.expression.get());
	}

	void Interpreter::visitVariable(VariableExpr &expr) {
		if (is_panicking)
			return;
		// Check if the Resolver found a specific depth for this expression
		auto it = locals.find(&expr);

		if (it != locals.end()) {
			int distance = it->second;
			if (distance != -1) {
				RyVariable &var = environment->getAt(distance, expr.name.lexeme);
				if (var.isPrivate && !environment->has(expr.name.lexeme, expr.name)) {
					throw "Cannot access private member '" + expr.name.lexeme + "'.";
				}
				lastResult = var.value;
			} else if (distance == -1) {
				globals->getVariable(expr.name);
				lastResult = globals->getVariable(expr.name).value;
			}

		} else {
			// If it's not in 'locals', it's a Global.
			// Get it from your top-level 'globals' environment.
			RyVariable &var = globals->getVariable(expr.name);
			lastResult = var.value;
		}
	}

	/// Visits a LogicalExpr node in the AST.
	///
	/// This node is used to implement logical operations between two expressions.
	///
	/// @param expr The LogicalExpr node to visit.
	///
	/// The result of the logical operation is stored in lastResult.
	void Interpreter::visitLogical(LogicalExpr &expr) {
		if (is_panicking)
			return;
		RyValue leftVal = evaluate(expr.left.get());
		bool leftTruth = isTruthy(leftVal);

		if (expr.op_t.type == TokenType::AND) {
			if (!leftTruth) {
				lastResult = false;
				return;
			}
			RyValue rightVal = evaluate(expr.right.get());
			lastResult = isTruthy(rightVal);
			return;
		}

		if (expr.op_t.type == TokenType::OR) {
			if (leftTruth) {
				lastResult = true;
				return;
			}
			RyValue rightVal = evaluate(expr.right.get());
			lastResult = isTruthy(rightVal);
			return;
		}
	}

	/// Visits an AssignExpr node in the AST.
	///
	/// This node is used to assign values to variables.
	///
	/// @param expr The AssignExpr node to visit.
	///
	/// @return The value of the assignment if it is a valid assign, or an RyRuntimeError if it is not.
	void Interpreter::visitAssign(AssignExpr &expr) {
		if (is_panicking)
			return;
		RyValue value = evaluate(expr.value.get());

		auto it = locals.find(&expr);
		if (it != locals.end()) {
			int distance = it->second;
			if (distance != -1) {
				RyVariable &var = environment->getAt(distance, expr.name.lexeme);
				// Check the rule
				if (var.typeConstraint.has_value()) {
					std::string constraint = var.typeConstraint.value();

					// Simple check: if constraint is "string", is the value a string?
					if (constraint == "string" && !value.isString()) {
						throw RyRuntimeError(expr.name, "Type Error: Cannot assign non-string to a string variable.");
					} else if (constraint == "num" && !value.isNumber()) {
						throw RyRuntimeError(expr.name, "Type Error: Cannot assign non-number to a number variable.");
					} else if (constraint == "bool" && !value.isBool()) {
						throw RyRuntimeError(expr.name, "Type Error: Cannot assign non-boolean to a boolean variable.");
					} else if (constraint == "list" && !value.isList()) {
						throw RyRuntimeError(expr.name, "Type Error: Cannot assign non-list to a list variable.");
					} else if (constraint == "map" && value.isMap()) {
						throw RyRuntimeError(expr.name, "Type Error: Cannot assign non-map to a map variable.");
					}
				}
				// Check if variable is in current environment
				if (var.isPrivate && !environment->has(expr.name.lexeme, expr.name)) {
					throw RyRuntimeError(expr.name, "Cannot access private member '" + expr.name.lexeme + "'.");
				}


				var.value = value;
			} else if (distance == -1) {
				globals->assign(expr.name, RyVariable{value, false, std::nullopt});
			}
		} else {
			globals->assign(expr.name, RyVariable{value, false, std::nullopt});
		}
		lastResult = value;
	}

	/// Visits a CallExpr node in the AST.
	///
	/// This node is used to call functions and classes.
	///
	/// @param expr The CallExpr node to visit.
	///
	/// @return The result of the function call if it is a valid call, or an RyRuntimeError if it is not.
	void Interpreter::visitCall(CallExpr &expr) {
		if (is_panicking)
			return;
		RyValue callee = evaluate(expr.callee.get());

		std::vector<RyValue> arguments;
		for (auto &arg: expr.arguments) {
			arguments.push_back(evaluate(arg.get()));
		}

		std::shared_ptr<RyCallable> function = nullptr;

		function = callee.asFunction();

		if (function != nullptr) {
			int minArgs = function->arity();
			int maxArgs = minArgs;

			// If it's a RyFunction, check the total parameter count
			auto ryFunc = std::dynamic_pointer_cast<RyFunction>(function);
			if (ryFunc) {
				maxArgs = ryFunc->declaration->parameters.size();
			}

			if (minArgs != -1) {
				if (arguments.size() < minArgs || arguments.size() > maxArgs) {
					std::string msg = (minArgs == maxArgs) ? "Expected " + std::to_string(minArgs) + " arguments."
																								 : "Expected between " + std::to_string(minArgs) + " and " +
																											 std::to_string(maxArgs) + " arguments.";

					throw RyRuntimeError(expr.Paren, msg + " but got " + std::to_string(arguments.size()) + ".");
				}
			}

			try {
				lastResult = function->call(*this, arguments);
			} catch (const std::runtime_error &e) {
				throw RyRuntimeError(expr.Paren, e.what());
			}
			return;
		}

		throw RyRuntimeError(expr.Paren, "Can only call functions and classes.");
	}

	void Interpreter::visitBitwiseOr(BitwiseOrExpr &expr) {
		if (is_panicking)
			return;
		auto left = evaluate(expr.left.get());
		auto right = evaluate(expr.right.get());


		if (!left.isNumber() || !right.isNumber()) {
			throw RyRuntimeError(expr.op_t, "Operands must be numbers.");
		}
		long l = (long) left.asNumber();
		long r = (long) right.asNumber();
		lastResult = (double) (l | r);
	}

	void Interpreter::visitBitwiseXor(BitwiseXorExpr &expr) {
		if (is_panicking)
			return;
		auto left = evaluate(expr.left.get());
		auto right = evaluate(expr.right.get());


		if (!left.isNumber() || !right.isNumber()) {
			throw RyRuntimeError(expr.op_t, "Operands must be numbers.");
		}
		long l = (long) left.asNumber();
		long r = (long) right.asNumber();
		lastResult = (double) (l ^ r);
	}

	void Interpreter::visitBitwiseAnd(BitwiseAndExpr &expr) {
		if (is_panicking)
			return;
		auto left = evaluate(expr.left.get());
		auto right = evaluate(expr.right.get());


		if (!left.isNumber() || !right.isNumber()) {
			throw RyRuntimeError(expr.op_t, "Operands must be numbers.");
		}
		long l = (long) left.asNumber();
		long r = (long) right.asNumber();
		lastResult = (double) (l & r);
	}


	void Interpreter::visitList(ListExpr &expr) {
		if (is_panicking)
			return;
		auto list = std::make_shared<std::vector<RyValue>>();

		for (const auto &element: expr.elements) {
			list->push_back(evaluate(element.get()));
		}

		lastResult = list; // lastResult is RyValue, so it holds the shared_ptr
	}

	/// Evaluates the given index expression, which is used to access elements of arrays.
	///
	/// This function will throw a RyRuntimeError if the index is out of bounds, or if the index is not a number.
	///
	/// @param expr The IndexExpr node to evaluate.
	///
	/// @return The value at the given index.
	void Interpreter::visitIndex(IndexExpr &expr) {
		if (is_panicking)
			return;
		RyValue callee = evaluate(expr.object.get());
		RyValue indexVal = evaluate(expr.index.get());

		if (callee.isList()) {
			auto list = callee.asList();

			if (indexVal.isNumber()) {
				int i = static_cast<int>(indexVal.asNumber());

				// Safety check! Use your new RyRuntimeError here
				if (i < 0 || i >= list->size()) {
					throw RyRuntimeError(expr.bracket, "Index out of bounds.");
				}

				lastResult = (*list)[i];
				return;
			}
			throw RyRuntimeError(expr.bracket, "Index must be a number.");
		} else if (callee.isMap()) {
			auto map = callee.asMap();

			if (indexVal.isString()) {
				std::string key = indexVal.asString();

				if (!map->has(key, expr.bracket)) {
					throw RyRuntimeError(expr.bracket, "Undefined property '" + key + "'.");
				}

				lastResult = map->get(key, expr.bracket);
				return;
			}

			throw RyRuntimeError(expr.bracket, "Index must be a string.");
		}


		throw RyRuntimeError(expr.bracket, "Only lists can be indexed.");
	}

	void Interpreter::visitIndexSet(IndexSetExpr &expr) {
		if (is_panicking)
			return;
		RyValue object = evaluate(expr.object.get());
		RyValue indexVal = evaluate(expr.index.get());
		RyValue value = evaluate(expr.value.get());

		if (object.isMap()) {
			auto map = object.asMap();

			if (indexVal.isString()) {
				std::string key = indexVal.asString();
				// We use define() here so that map["new_key"] = val
				// creates the key if it doesn't exist!
				map->define(key, value);
				lastResult = value;
				return;
			}
			throw RyRuntimeError(expr.bracket, "Map index must be a string.");
		}

		// Don't forget lists! user[0] = "new value"
		if (object.isList()) {
			auto list = object.asList();
			if (indexVal.isNumber()) {
				int i = static_cast<int>(indexVal.asNumber());
				if (i < 0 || i >= list->size())
					throw RyRuntimeError(expr.bracket, "Index out of bounds.");

				(*list)[i] = value;
				lastResult = value;
				return;
			}
			throw RyRuntimeError(expr.bracket, "List index must be a number.");
		}

		throw RyRuntimeError(expr.bracket, "Only lists and maps support indexed assignment.");
	}
	void Interpreter::visitThis(ThisExpr &expr) {
		if (is_panicking)
			return;
		lastResult = environment->get(expr.keyword);
	}
	/// Evaluates the given block of statements in the given scope.
	void Interpreter::visitBlockStmt(BlockStmt &stmt) {
		if (is_panicking)
			return;
		auto localEnv = std::make_shared<Environment>(this->environment);
		executeBlock(stmt.statements, *localEnv);
	}

	void Interpreter::visitExpressionStmt(ExpressionStmt &stmt) {
		if (is_panicking)
			return;
		evaluate(stmt.expression.get());
	}

	/// Evaluates the expression part of a FunctionStmt and defines it in the current scope.
	void Interpreter::visitFunctionStmt(FunctionStmt &stmt) {
		if (is_panicking)
			return;
		auto stmtPtr = std::static_pointer_cast<FunctionStmt>(stmt.shared_from_this());

		auto internalFunc =
				std::make_shared<RyFunction>(stmtPtr, std::make_shared<Environment>(this->environment), stmt.isPrivate);

		std::shared_ptr<RyCallable> callable = std::static_pointer_cast<RyCallable>(internalFunc);

		// Define it in the current scope
		environment->define(stmt.name.lexeme, callable);
	}
	/// Evaluates the expression part of an ImportStmt and adds the module to the current scope's loaded modules.
	void Interpreter::visitImportStmt(ImportStmt &stmt) {
		if (is_panicking)
			return;
		std::string moduleName = stmt.module.lexeme;

		// Basic Sanitization
		if (moduleName.front() == '"')
			moduleName = moduleName.substr(1, moduleName.length() - 2);
		if (moduleName.find('*') != std::string::npos) {
			// Strip the "/*.ry" or "/*" to get the folder name
			std::string folderName = moduleName;
			size_t starPos = folderName.find('*');
			if (starPos != std::string::npos) {
				folderName = folderName.substr(0, starPos);
				if (!folderName.empty() && (folderName.back() == '/' || folderName.back() == '\\')) {
					folderName.pop_back(); // Remove trailing slash
				}
			}

			// Find the actual directory
			std::string dirPath = RyTools::findModulePath(folderName, true);

			if (!dirPath.empty() && fs::is_directory(dirPath)) {
				for (const auto &entry: fs::directory_iterator(dirPath)) {
					if (entry.path().extension() == ".ry") {
						std::string content = readFile(entry.path().string());
						interpretSource(content);
					}
				}
			} else {
				std::cerr << "Directory '" << folderName << "' not found for wildcard import.\n";
			}
			return;
		}
		// Prevent Circular Imports
		if (loaded_modules.contains(moduleName))
			return;
		loaded_modules.insert(moduleName);

		// Path Resolution (Using your RyTools helper)
		std::string sourcePath = RyTools::findModulePath(moduleName);
		if (sourcePath.empty()) {
			std::cerr << "Module '" << moduleName << "' not found.\n";
			return;
		}

		try {
			// Load and Parse
			const std::string source = readFile(sourcePath);
			Backend::Lexer moduleLexer(source);
			const auto moduleTokens = moduleLexer.scanTokens();
			Resolver moduleResolver(*this);
			Backend::Parser moduleParser(moduleTokens, typeAliases, source);

			// Get the full list of statements
			auto moduleStmts = moduleParser.parse();


			moduleResolver.resolve(moduleStmts);

			// Execute in the Global Scope
			// We temporarily switch to 'globals' so the module's
			// functions/namespaces live at the top level.
			std::shared_ptr<Environment> previous = this->environment;
			this->environment = globals;

			try {
				for (const auto &s: moduleStmts) {
					execute(s.get());
				}
			} catch (...) {
				this->environment = previous;
				throw;
			}

			// Restore original environment
			this->environment = previous;

		} catch (const std::exception &e) {
			std::cerr << "Error loading " << moduleName << ".ry: " << e.what() << "\n";
		}
	}

	/// Evaluates the expression part of a VarStmt and defines it in the current scope.
	void Interpreter::visitVarStmt(VarStmt &stmt) {
		if (is_panicking)
			return;
		std::string constraint = "";

		// Handle the raw 'data' keyword (data::num)
		if (stmt.type.lexeme == "data" && stmt.innerType.has_value()) {
			constraint = stmt.innerType->lexeme;
		}
		// Handle namespaced types (Math.int) or global aliases
		else {
			// If innerType exists, use it as the alias (e.g., 'int' in Math.int)
			// Otherwise, the type itself is the alias (e.g., 'num' in num x)
			Token aliasToken = stmt.innerType.has_value() ? stmt.innerType.value() : stmt.type;

			std::optional<Token> prefix = stmt.innerType.has_value() ? std::make_optional(stmt.type) : std::nullopt;

			constraint = resolveType(prefix, aliasToken);
		}

		RyValue value = nullptr;
		if (stmt.initializer)
			value = evaluate(stmt.initializer.get());

		if (!constraint.empty()) {
			checkType(stmt.name, constraint, value);
		}

		RyVariable var;
		var.value = value;
		var.isPrivate = stmt.isPrivate;
		var.typeConstraint = constraint;
		environment->define(stmt.name.lexeme, var);
	}

	/// Evaluates the expression part of an AliasStmt and defines it in the current scope.
	void Interpreter::visitAliasStmt(AliasStmt &stmt) {
		if (is_panicking)
			return;
		// Check if the target is a type alias (data::num or num)
		if (isTypeAlias(stmt.aliasExpr)) {
			std::string originalType = getTypeName(stmt.aliasExpr);

			// Tell the Environment (for runtime logic)
			environment->defineTypeAlias(stmt.name.lexeme, originalType);

			// Tell the Parser's shared set (for future parsing)
			// This ensures the next REPL line knows the new name is a type
			typeAliases.insert(stmt.name.lexeme);

			return; // Exit early! Do NOT call evaluate() on a type.
		}

		// Normal variable/function aliasing
		// Only call evaluate if it's a real value, not a type name
		RyValue value = evaluate(stmt.aliasExpr.get());
		environment->define(stmt.name.lexeme, value);
	}

	/// Evaluates the expression part of a ReturnStmt and throws a ReturnSignal with the evaluated value.
	void Interpreter::visitReturnStmt(ReturnStmt &stmt) {
		if (is_panicking)
			return;
		RyValue value = nullptr;
		if (stmt.value != nullptr) {
			value = evaluate(stmt.value.get());
		}

		throw ReturnSignal(value);
	}

	/// Evaluates the expression part of a WhileStmt and executes either the then or else branch accordingly.
	/// @param stmt The WhileStmt node to visit.
	/// @return void if the condition is true, or an empty value if the condition is false.
	void Interpreter::visitWhileStmt(WhileStmt &stmt) {
		if (is_panicking)
			return;
		while (isTruthy(evaluate(stmt.condition.get()))) {
			try {
				execute(stmt.body.get());
			} catch (const StopSignal &s) {
				break;
			} catch (const SkipSignal &s) {
				continue;
			}
		}
	}


	/// Evaluates the expression part of an IfStmt and executes either the then or else branch accordingly.
	/// @param stmt The IfStmt node to visit.
	/// @return void if the condition is true, or an empty value if the condition is false.
	void Interpreter::visitIfStmt(IfStmt &stmt) {
		if (is_panicking)
			return;
		if (RyValue conditionValue = evaluate(stmt.condition.get()); isTruthy(conditionValue)) {
			execute(stmt.thenBranch.get());
		} else if (stmt.elseBranch != nullptr) {
			execute(stmt.elseBranch.get());
		}
	}

	/// Evaluates the expression part of a NamespaceStmt and defines it in the current scope.
	/// A NamespaceStmt defines a new "Module" environment and executes the namespace body in this new environment.
	/// The namespace body is executed in the new environment, and the environment itself is stored as the value of the
	/// namespace. The namespace is defined in the current scope, and the environment is stored as the value.
	void Interpreter::visitNamespaceStmt(NamespaceStmt &stmt) {
		if (is_panicking)
			return;
		auto namespaceEnv = std::make_shared<Environment>(this->environment);

		std::shared_ptr<Environment> previous = this->environment;
		this->environment = namespaceEnv;

		try {
			for (const auto &s: stmt.body) {
				execute(s.get());
			}
		} catch (...) {
			this->environment = previous;
			throw;
		}

		this->environment = previous;
		environment->define(stmt.name.lexeme, namespaceEnv);
	}
	void Interpreter::visitEachStmt(EachStmt &stmt) {
		if (is_panicking)
			return;
		RyValue collection = evaluate(stmt.collection.get());

		if (!collection.isList()) {
			throw RyRuntimeError(stmt.id, "The 'each' loop requires a list.");
		}

		auto list = collection.asList();

		// We save the previous environment to restore it after the loop
		std::shared_ptr<Environment> previous = this->environment;

		try {
			for (const auto &item: *list) {
				auto loopEnv = std::make_shared<Environment>(previous);
				loopEnv->define(stmt.id.lexeme, item);

				try {
					this->environment = loopEnv;
					execute(stmt.body.get());
				} catch (const SkipSignal &) {
					continue;
				} catch (const StopSignal &) {
					break;
				}
			}
		} catch (...) {
			this->environment = previous;
			throw;
		}

		this->environment = previous;
	}
	void Interpreter::visitStopStmt(StopStmt &stmt) {
		if (is_panicking)
			return;
		throw StopSignal();
	}
	void Interpreter::visitSkipStmt(SkipStmt &stmt) {
		if (is_panicking)
			return;
		throw SkipSignal();
	}
	void Interpreter::visitForStmt(ForStmt &stmt) {
		if (is_panicking)
			return;
		auto loopEnv = std::make_shared<Environment>(this->environment);
		std::shared_ptr<Environment> previous = this->environment;
		this->environment = loopEnv;

		try {
			if (stmt.init)
				execute(stmt.init.get());

			while (isTruthy(evaluate(stmt.condition.get()))) {
				try {
					execute(stmt.body.get());
				} catch (const SkipSignal &) {
				} catch (const StopSignal &) {
					break; // Kill the whole loop
				}
				if (stmt.increment) {
					evaluate(stmt.increment.get());
				}
			}
		} catch (...) {
			this->environment = previous;
			throw;
		}
		this->environment = previous;
	}
	void Interpreter::visitClassStmt(ClassStmt &stmt) {
		if (is_panicking)
			return;
		RyValue superVal = nullptr;
		std::shared_ptr<RyClass> superclass = nullptr;
		if (stmt.superclass != nullptr) {
			superVal = evaluate(stmt.superclass.get());
			if (!superVal.isClass()) {
				throw RyRuntimeError(stmt.superclass->name, "Superclass must be a class.");
			}
			superclass = std::dynamic_pointer_cast<RyClass>(superVal.asFunction());
		}

		std::unordered_map<std::string, std::shared_ptr<RyFunction>> methods;
		std::unordered_map<std::string, RyVariable> fieldBlueprints;

		std::shared_ptr<Environment> methodEnv = environment;
		if (superclass) {
			methodEnv = std::make_shared<Environment>(environment);
			methodEnv->define("parent", superVal);
		}

		// Collect Fields (Data)
		for (auto &field: stmt.fields) {
			RyValue initialValue = nullptr;
			if (field->initializer) {
				initialValue = evaluate(field->initializer.get());
			}

			// Create the variable and STAMP it with the privacy from the statement
			RyVariable var(initialValue, field->isPrivate);
			fieldBlueprints[field->name.lexeme] = var;
		}

		// Collect Methods
		for (auto &method: stmt.methods) {
			auto function = std::make_shared<RyFunction>(method, methodEnv, method->isPrivate);
			methods[method->name.lexeme] = function;
		}

		auto klass = std::make_shared<RyClass>(stmt.name.lexeme, methods, fieldBlueprints, superclass);
		environment->define(stmt.name.lexeme, std::static_pointer_cast<RyCallable>(klass));
	}
	void Interpreter::visitAttemptStmt(AttemptStmt &stmt) {
		bool was_panicking = is_panicking;
		is_panicking = false;

		bool caught = false; // Track if it handled the error

		try {
			auto attemptEnv = std::make_shared<Environment>(this->environment);
			executeBlock(stmt.attemptBody, *attemptEnv);
		} catch (const RyRuntimeError &error) {
			// Check if it should catch this specific error
			bool isCatchAll = stmt.errorType.lexeme.empty() || stmt.errorType.type == TokenType::Nothing_Here;
			bool typeMatch = (error.type.isString() && error.type.asString() == stmt.errorType.lexeme);

			if (isCatchAll || typeMatch) {
				is_panicking = false; // Stop the engine from panicking
				auto failEnv = std::make_shared<Environment>(this->environment);
				failEnv->define(stmt.error.lexeme, error.message);
				executeBlock(stmt.failBody, *failEnv);
				caught = true; // Mark as handled!
			} else {
				// Not the type. Run finally because 'throw' exits the function immediately
				if (!stmt.finallyBody.empty()) {
					executeBlock(stmt.finallyBody, *environment);
				}
				throw;
			}
		}

		if (!caught && !stmt.finallyBody.empty()) {
			executeBlock(stmt.finallyBody, *environment);
		}

		// Only restore the old panic state if it NEVER caught an error.
		// If caught it, is_panicking stays FALSE so the program continues.
		if (!caught) {
			is_panicking = was_panicking;
		}
	}

	void Interpreter::visitPanicStmt(PanicStmt &stmt) {
		RyValue message;
		if (stmt.message != nullptr)
			message = evaluate(stmt.message.get());

		throw RyRuntimeError(stmt.keyword, message.toString(), true);
	}
} // namespace Frontend
