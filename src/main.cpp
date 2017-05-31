#include <stdio.h>

#include "../CppUtils/disc_union.h"
#include "../CppUtils/strings.h"
#include "../CppUtils/sexpr.h"

struct LispValue;

typedef void (BuiltinFuncOp)(LispValue*, int, LispValue*);

struct LispLambdaValue {
	Vector<SubString> argNames;
	BNSexpr* body;
};

struct LispBuiltinFuncValue {
	BuiltinFuncOp* func;
	LispBuiltinFuncValue(BuiltinFuncOp* _func) { func = _func; }
};

typedef BNSexprNumber     LispNumValue;
typedef BNSexprString     LispStringValue;
typedef BNSexprIdentifier LispIdentifierValue;

struct LispVoidValue {

};

#define DISC_MAC(mac)    \
	mac(LispLambdaValue) \
	mac(LispBuiltinFuncValue) \
	mac(LispNumValue) \
	mac(LispIdentifierValue) \
	mac(LispVoidValue) \
	mac(LispStringValue)

DEFINE_DISCRIMINATED_UNION(LispValue, DISC_MAC)

#undef DISC_MAC

#define MATH_BUILTIN_OP(name, op)                                                        \
			void MathBuiltin_ ## name (LispValue* vals, int count, LispValue* outVal) {  \
				ASSERT(count == 2);                                                      \
				ASSERT(vals[0].IsLispNumValue());                                        \
				ASSERT(vals[1].IsLispNumValue());                                        \
				BNSexprNumber num = 0.0;                                                 \
				if (vals[0].AsLispNumValue().isFloat || vals[1].AsLispNumValue().isFloat) {                  \
					num = vals[0].AsLispNumValue().CoerceDouble() op vals[1].AsLispNumValue().CoerceDouble();\
				}              \
				else {         \
					num = vals[0].AsLispNumValue().iValue op vals[1].AsLispNumValue().iValue;                \
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

struct LispBinding {
	String name;
	LispValue value;
};

struct LispEvalContext {
	Vector<LispValue> evalStack;
	Vector<LispBinding> bindings;
	LispEvalContext() {
		for (int i = 0; i < BNS_ARRAY_COUNT(defaultBindings); i++) {
			LispValue val;
			val = LispBuiltinFuncValue(defaultBindings[i].func);

			LispBinding bind;
			bind.name = defaultBindings[i].name;
			bind.value = val;

			bindings.PushBack(bind);
		}
	}
};

LispValue GetBindingForIdentifier(const SubString& name, LispEvalContext* ctx) {
	BNS_VEC_FOREACH(ctx->bindings) {
		if (name == ptr->name) {
			return ptr->value;
		}
	}

	LispValue val;
	val = LispVoidValue();
	return val;
}

LispValue GetBindingForIdentifier(BNSexpr* sexpr, LispEvalContext* ctx) {
	ASSERT(sexpr->IsBNSexprIdentifier());
	return GetBindingForIdentifier(sexpr->AsBNSexprIdentifier().identifier, ctx);
}

void EvalSexpr(BNSexpr* sexpr, LispEvalContext* ctx) {
	if (sexpr->IsBNSexprParenList()) {
		int idx = ctx->evalStack.count;
		BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children){
			EvalSexpr(ptr, ctx);
		}

		if (ctx->evalStack.count > idx) {
			LispValue func = ctx->evalStack.data[idx];
			if (func.IsLispBuiltinFuncValue()) {
				LispValue result;
				func.AsLispBuiltinFuncValue().func(&ctx->evalStack.data[idx + 1], ctx->evalStack.count - idx - 1, &result);
				ctx->evalStack.RemoveRange(idx, ctx->evalStack.count);
				ctx->evalStack.PushBack(result);
			}
			else if (func.IsLispLambdaValue()) {
				// TODO:
				ASSERT(false);
			}
			else {
				ASSERT(false);
			}
		}
		else {
			ASSERT(false);
		}
	}
	else if (sexpr->IsBNSexprIdentifier()) {
		LispValue val = GetBindingForIdentifier(sexpr, ctx);
		ctx->evalStack.PushBack(val);
	}
	else if (sexpr->IsBNSexprNumber()) {
		LispValue val;
		val = sexpr->AsBNSexprNumber();
		ctx->evalStack.PushBack(val);
	}
	else if (sexpr->IsBNSexprString()) {
		LispValue val;
		val = sexpr->AsBNSexprString();
		ctx->evalStack.PushBack(val);
	}
	else {
		ASSERT(false);
	}
}

void EvalSexprs(Vector<BNSexpr>* sexprs, LispEvalContext* ctx) {
	BNS_VEC_FOREACH(*sexprs) {
		EvalSexpr(ptr, ctx);
	}
}

void PrintLispValue(LispValue* val, FILE* file = stdout) {
	if (val->IsLispStringValue()) {
		fprintf(file, "\"%.*s\"", BNS_LEN_START(val->AsLispStringValue().value));
	}
	else if (val->IsLispNumValue()) {
		if (val->AsLispNumValue().isFloat) {
			fprintf(file, "%f", val->AsLispNumValue().fValue);
		}
		else {
			fprintf(file, "%lld", val->AsLispNumValue().iValue);
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
			PrintLispValue(ptr);
			printf("\n");
		}

		ctx.evalStack.Clear();
	}

	return 0;
}