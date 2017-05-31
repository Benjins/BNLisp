#include <stdio.h>

#include "../CppUtils/disc_union.h"
#include "../CppUtils/strings.h"
#include "../CppUtils/sexpr.h"

typedef void (BuiltinFuncOp)(BNSexpr**, int, BNSexpr*);

#define MATH_BUILTIN_OP(name, op)                                                    \
			void MathBuiltin_ ## name (BNSexpr** vals, int count, BNSexpr* outVal) { \
				ASSERT(count == 2);                                                  \
				ASSERT(vals[0]->IsBNSexprNumber());                                  \
				ASSERT(vals[1]->IsBNSexprNumber());                                  \
				BNSexprNumber num = 0.0;                                                   \
				if (vals[0]->AsBNSexprNumber().isFloat || vals[1]->AsBNSexprNumber().isFloat) {                  \
					num = vals[0]->AsBNSexprNumber().CoerceDouble() op vals[1]->AsBNSexprNumber().CoerceDouble();\
				}              \
				else {         \
					num = vals[0]->AsBNSexprNumber().iValue op vals[1]->AsBNSexprNumber().iValue;                \
				}              \
				*outVal = num; \
			}

MATH_BUILTIN_OP(Mul, *)
MATH_BUILTIN_OP(Div, /)
MATH_BUILTIN_OP(Add, +)
MATH_BUILTIN_OP(Sub, -)

struct BuiltinBinding {
	const char* name;
	BuiltinFuncOp* func;
};

BuiltinBinding defaultBindings[] = {
	{ "*", MathBuiltin_Mul },
	{ "/", MathBuiltin_Div },
	{ "+", MathBuiltin_Add },
	{ "-", MathBuiltin_Sub }
};

struct LispEvalContext {
	Vector<BNSexpr*> evalStack;
	Vector<BuiltinBinding> builtinBindings;
	Vector<BNSexpr> evalAllocs;
	LispEvalContext() {
		builtinBindings.InsertArray(0, defaultBindings, BNS_ARRAY_COUNT(defaultBindings));

		// TODO: Eval needs to be better than this...
		evalAllocs.EnsureCapacity(1000 * 1000);
	}
};

BuiltinFuncOp* GetBuiltinBindingForIdentifier(BNSexpr* sexpr, LispEvalContext* ctx) {
	ASSERT(sexpr->IsBNSexprIdentifier());
	BNS_VEC_FOREACH(ctx->builtinBindings) {
		if (sexpr->AsBNSexprIdentifier().identifier == ptr->name) {
			return ptr->func;
		}
	}

	return nullptr;
}

void EvalSexpr(BNSexpr* sexpr, LispEvalContext* ctx) {
	if (sexpr->IsBNSexprParenList()) {
		int idx = ctx->evalStack.count;
		BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children){
			EvalSexpr(ptr, ctx);
		}

		if (ctx->evalStack.count > idx && ctx->evalStack.data[idx]->IsBNSexprIdentifier()) {
			BuiltinFuncOp* func = GetBuiltinBindingForIdentifier(ctx->evalStack.data[idx], ctx);
			ASSERT(func != nullptr);
			BNSexpr result;
			func(&ctx->evalStack.data[idx + 1], ctx->evalStack.count - idx - 1, &result);
			ctx->evalStack.RemoveRange(idx, ctx->evalStack.count);
			ctx->evalAllocs.PushBack(result);
			BNSexpr* stkResult = &ctx->evalAllocs.Back();
			ctx->evalStack.PushBack(stkResult);
		}
		else {
			ASSERT(false);
		}
	}
	else {
		ctx->evalStack.PushBack(sexpr);
	}
}

void EvalSexprs(Vector<BNSexpr>* sexprs, LispEvalContext* ctx) {
	BNS_VEC_FOREACH(*sexprs) {
		EvalSexpr(ptr, ctx);
	}
}

void PrintSexpr(BNSexpr* sexpr, FILE* file = stdout) {
	if (sexpr->IsBNSexprString()) {
		fprintf(file, "\"%.*s\"", BNS_LEN_START(sexpr->AsBNSexprString().value));
	}
	else if (sexpr->IsBNSexprNumber()) {
		if (sexpr->AsBNSexprNumber().isFloat) {
			fprintf(file, "%f", sexpr->AsBNSexprNumber().fValue);
		}
		else {
			fprintf(file, "%lld", sexpr->AsBNSexprNumber().iValue);
		}
	}
	else {
		// TODO
		ASSERT(false);
	}
}

#include "../CppUtils/strings.cpp"
#include "../CppUtils/assert.cpp"
#include "../CppUtils/vector.cpp"
#include "../CppUtils/sexpr.cpp"

int main(){
	LispEvalContext ctx;
	while (true) {
		printf("Enter something:\n");
		char userIn[256];
		fgets(userIn, sizeof(userIn), stdin);
		userIn[StrLen(userIn) - 1] = '\0';

		Vector<BNSexpr> sexprs;
		ParseSexprs(&sexprs, userIn);

		ASSERT(sexprs.count == 1);

		EvalSexprs(&sexprs, &ctx);

		ASSERT(ctx.evalStack.count == sexprs.count);

		BNS_VEC_FOREACH(ctx.evalStack) {
			PrintSexpr(*ptr);
			printf("\n");
		}

		ctx.evalStack.Clear();
		ctx.evalAllocs.Clear();
	}

	return 0;
}