#include <stdio.h>

#include "../CppUtils/disc_union.h"
#include "../CppUtils/strings.h"
#include "../CppUtils/sexpr.h"

struct LispValue;

typedef void (BuiltinFuncOp)(LispValue*, int, LispValue*);

struct LispLambdaValue {
	Vector<SubString> argNames;
	BNSexpr body;
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
				LispNumValue num = 0.0;                                                  \
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

void StringBuiltin_cmp(LispValue* vals, int count, LispValue* outVal) {
	ASSERT(count == 2);
	ASSERT(vals[0].IsLispStringValue());
	ASSERT(vals[1].IsLispStringValue());
	int len = BNS_MIN(vals[0].AsLispStringValue().value.length, vals[1].AsLispStringValue().value.length);
	LispNumValue num = strncmp(vals[0].AsLispStringValue().value.start, vals[1].AsLispStringValue().value.start, len);
	*outVal = num;
}

struct BuiltinBinding {
	const char* name;
	BuiltinFuncOp* func;
};

BuiltinBinding defaultBindings[] = {
	{ "*", MathBuiltin_Mul },
	{ "/", MathBuiltin_Div },
	{ "+", MathBuiltin_Add },
	{ "-", MathBuiltin_Sub },
	{ "strcmp", StringBuiltin_cmp }
};

struct LispBinding {
	String name;
	LispValue value;
};

struct LispEvalContext {
	Vector<LispValue> evalStack;
	Vector<LispBinding> bindings;

	Vector<int> bindingCountFrames;

	void PushFrame() {
		bindingCountFrames.PushBack(bindings.count);
	}

	void PopFrame() {
		int prevCount = bindingCountFrames.Back();
		bindingCountFrames.PopBack();
		ASSERT(prevCount <= bindings.count);
		bindings.RemoveRange(prevCount, bindings.count);
	}

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
	for (int i = ctx->bindings.count - 1; i >= 0; i--) {
		if (name == ctx->bindings.data[i].name) {
			return ctx->bindings.data[i].value;
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

		const Vector<BNSexpr>& children = sexpr->AsBNSexprParenList().children;
		bool specialCase = false;
		if (children.count > 0) {
			if (children.data[0].IsBNSexprIdentifier()
			 && children.data[0].AsBNSexprIdentifier().identifier == "define") {
				specialCase = true;
				if (children.count == 3) {
					if (children.data[1].IsBNSexprIdentifier()) {
						EvalSexpr(&children.data[2], ctx);
						LispValue val = ctx->evalStack.Back();
						ctx->evalStack.PopBack();
						LispBinding binding;
						binding.name = children.data[1].AsBNSexprIdentifier().identifier;
						binding.value = val;
						ctx->bindings.PushBack(binding);
					}
					else if (children.data[1].IsBNSexprParenList()) {
						LispLambdaValue val;
						const Vector<BNSexpr>& grandChildren = children.data[1].AsBNSexprParenList().children;
						if (grandChildren.count > 0) {
							bool allIdentifiers = true;
							BNS_VEC_FOREACH(grandChildren) {
								if (!ptr->IsBNSexprIdentifier()) {
									allIdentifiers = false;
									break;
								}
							}

							if (allIdentifiers) {
								LispBinding binding;
								binding.name = grandChildren.data[0].AsBNSexprIdentifier().identifier;

								val.argNames.EnsureCapacity(grandChildren.count - 1);
								for (int i = 1; i < grandChildren.count; i++) {
									val.argNames.PushBack(grandChildren.data[i].AsBNSexprIdentifier().identifier);
								}
								val.body = children.data[2];
								binding.value = val;
								ctx->bindings.PushBack(binding);
							}
							else {
								ASSERT(false);
							}
						}
						else {
							ASSERT(false);
						}
					}
					else {
						ASSERT(false);
					}
				}
				else {
					ASSERT(false);
				}
			}
			else if (children.data[0].IsBNSexprIdentifier()
				&& children.data[0].AsBNSexprIdentifier().identifier == "begin") {
				specialCase = true;
				ctx->PushFrame();

				int stackCount = ctx->evalStack.count;
				for (int i = 1; i < children.count - 1; i++) {
					EvalSexpr(&children.data[i], ctx);
					ctx->evalStack.RemoveRange(stackCount, ctx->evalStack.count);
				}
				
				EvalSexpr(&children.data[children.count - 1], ctx);

				ctx->PopFrame();
			}
			else if (children.data[0].IsBNSexprIdentifier()
				&& children.data[0].AsBNSexprIdentifier().identifier == "if") {
				// TODO: booleans and stuff
			}
		}

		if (!specialCase) {
			int idx = ctx->evalStack.count;
			BNS_VEC_FOREACH(sexpr->AsBNSexprParenList().children) {
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
					int arity = func.AsLispLambdaValue().argNames.count;
					int argCount = ctx->evalStack.count - idx - 1;
					if (arity == argCount) {
						int prevCount = ctx->bindings.count;
						for (int i = 0; i < arity; i++) {
							LispBinding binding;
							binding.name = func.AsLispLambdaValue().argNames.data[i];
							binding.value = ctx->evalStack.data[idx + 1 + i];
							ctx->bindings.PushBack(binding);
						}

						ctx->evalStack.RemoveRange(idx, ctx->evalStack.count);

						EvalSexpr(&func.AsLispLambdaValue().body, ctx);

						ctx->bindings.RemoveRange(prevCount, ctx->bindings.count);
					}
					else {
						ASSERT(false);
					}
				}
				else if (func.IsLispVoidValue()) {
					printf("Error, unbound identifier\n");
				}
				else {
					ASSERT(false);
				}
			}
			else {
				ASSERT(false);
			}
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

		if (StrEqual(userIn, "!quit")) {
			break;
		}

		Vector<BNSexpr> sexprs;
		ParseSexprs(&sexprs, userIn);

		EvalSexprs(&sexprs, &ctx);

		BNS_VEC_FOREACH(ctx.evalStack) {
			PrintLispValue(ptr);
			printf("\n");
		}

		ctx.evalStack.Clear();
	}

	return 0;
}