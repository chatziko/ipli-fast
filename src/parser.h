
#pragma once

#include <ADTVector.h>
#include <ADTMap.h>
#include <ADTSet.h>


extern int* memory;

typedef char* String;

typedef Vector Program;	// Vector of Statement

typedef enum {
	WRITE,			// write <var1>
	WRITELN,		// writeln <var1>
	READ,			// read <var1>
	ASSIGN_VAR,		// <var1> = <var2>
	ASSIGN_EXP,		// <var1> = <var2> <op> <var3>
	WHILE,			// while <var1> <op> <var2>  \n<body>
	IF,				// if <var1> <op> <var2>  \n<body>  \nelse  \n<body>
	RAND,			// random <var1>
	ARG_SIZE,		// argument size <var1>
	ARG,			// argument <var1> <var2>
	BREAK,			// break <var1>
	CONTINUE,		// continue <var1>
	NEW,			// new <array>[var1]
	FREE,			// free <array>
	SIZE,			// size <array> <var1>
} StatementType;

typedef int* Array;

typedef enum {
	OP_WRITE = 0,		// write reg1
	OP_WRITELN,			// writeln reg1
	OP_READ,			// reg1 = read
	OP_LOAD1_V,			// reg1 = <var>
	OP_LOAD1_A,			// reg1 = <array>[<var>]
	OP_LOAD2_V,			// reg2 = <var>
	OP_LOAD2_A,			// reg2 = <array>[<var>]
	OP_STORE_V,			// <var> = reg1
	OP_STORE_A,			// <array>[<var>] = reg1
	OP_ASSIGN_VV,
	OP_ASSIGN_VA,
	OP_ASSIGN_AV,
	OP_ASSIGN_AA,
	OP_INC_V,			// <var>++
	OP_INC_A,			// <array>[<var>]++
	OP_DEC_V,			// <var>--
	OP_DEC_A,			// <array>[<var>]--
	OP_JUMP,			// jump <n>
	OP_RAND,			// reg1 = random
	OP_NEW,				// <array> = malloc reg1
	OP_FREE,			// free <array>
	OP_SIZE,			// reg1 = size <array>
	OP_HALT,			// stop execution

	OP_ADD_VVV,			// var3 = var1 + var2
	OP_ADD_VVA,			// var3 = var1 + <arr2>[var2]
	OP_ADD_VAA,			// <arr1>[<var1>] = <arr2>[var2] + <arr3>[var3]
	OP_ADD_AVV,			// <arr1>[<var1>] = <arr2>[var2] + <arr3>[var3]
	OP_ADD_AVA,			// <arr1>[<var1>] = <arr2>[var2] + <arr3>[var3]
	OP_ADD_AAA,			// <arr1>[<var1>] = <arr2>[var2] + <arr3>[var3]
	OP_SUB_VVV,			// <arr1>[<var1>] = <arr2>[var2] - <arr3>[var3]
	OP_SUB_VVA,			// <arr1>[<var1>] = <arr2>[var2] - <arr3>[var3]
	OP_SUB_VAA,			// <arr1>[<var1>] = <arr2>[var2] - <arr3>[var3]
	OP_SUB_AVV,			// <arr1>[<var1>] = <arr2>[var2] - <arr3>[var3]
	OP_SUB_AVA,			// <arr1>[<var1>] = <arr2>[var2] - <arr3>[var3]
	OP_SUB_AAA,			// <arr1>[<var1>] = <arr2>[var2] - <arr3>[var3]
	OP_MUL,				// reg1 = reg1 * reg2
	OP_DIV,				// reg1 = reg1 / reg2
	OP_MOD,				// reg1 = reg1 % reg2

	OP_EQ_VV,			// jump if not <var1> == <var2>
	OP_EQ_VA,			// jump if not <var1> == array[<var2>]
	OP_EQ_AA,			// jump if not array1[<var1>] == array2[<var2>]
	OP_NEQ_VV,			// jump if not <var1> != <var2>
	OP_NEQ_VA,			// jump if not <var1> != array[<var2>]
	OP_NEQ_AA,			// jump if not array1[<var1>] != array2[<var2>]
	OP_LE_VV,			// jump if not var1 <= var
	OP_LE_VA,			// jump if not reg1 <= reg2
	OP_LE_AV,			// jump if not reg1 <= reg2
	OP_LE_AA,			// jump if not reg1 <= reg2
	OP_LT_VV,			// jump if not var1 < var
	OP_LT_VA,			// jump if not reg1 < reg2
	OP_LT_AV,			// jump if not reg1 < reg2
	OP_LT_AA,			// jump if not reg1 < reg2
} Opcode;

typedef struct bc_instruction {
	Opcode opcode;
	int n;
	int* args[6];
	int arg_n;
	void** thread_pos;
	int exec_count;
}* BCInstruction;

typedef struct {
	Vector code;
	Set allocs;			// set of alloced memory
	bool verbose;

	// only used during parsing
	Map variables;		// name => int
	Map arrays;			// name => Vector of int
}* Runtime;

typedef struct statement {
	StatementType type;
	String tokens[6];
	Program body, else_body;
	int start_pos, end_pos;		// start/end position in code
}* Statement;


// Parses a source file (vector of strings) into a program

Runtime parser_create_runtime(Vector source, Vector args, bool verbose);

void parser_destroy_runtime(Runtime runtime);

int* parser_alloc(int int_n, Runtime runtime);
void parser_free(Pointer p, Runtime runtime);