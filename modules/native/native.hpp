#pragma once
#include "runtime.h"

typedef RyValue (*RawNativeFn)(std::vector<RyValue>);
typedef void (*RegisterFn)(const char *name, RawNativeFn func, void *target);

namespace Frontend {
	class ExternalNative : public RyCallable {
		RawNativeFn func;

	public:
		ExternalNative(RawNativeFn f) : func(f) {}

		int arity() override { return -1; } // Flexible arity for external libs

		RyValue call(Interpreter &i, std::vector<RyValue> args) override { return func(args); }
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}
		std::string toString() override { return "<external native fn>"; }
	};
	// A plain C function that the .so can call
	inline void register_callback(const char *name, RawNativeFn func, void *target) {
		// Cast target to Environment
		auto *env = static_cast<Backend::Environment *>(target);

		if (env) {
			auto envPtr = std::shared_ptr<Backend::Environment>(env, [](auto *) {});
			auto wrapper = std::static_pointer_cast<RyCallable>(std::make_shared<ExternalNative>(func));
			env->define(name, wrapper);
		}
	}
} // namespace Frontend
