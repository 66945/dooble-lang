#pragma once

#include "../../type.h"
#include "../../internal.h"

typedef PAIR(string_t, CType) TypePair;
typedef struct {
	VEC(TypePair);
	bool is_union;
} AnonStruct;

typedef struct {
	CodeGen codegen;

	// type information
	VEC(AnonStruct) anon_structs;
	HashMap         type_map;
} GenCompiler;

void  init_compiler(GenCompiler *compiler);
CType build_type(GenCompiler *comp, typeid id);
