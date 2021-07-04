
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "interpreter.h"



// these should be called for all memory allocated for
// the program's variables and arrays.
int* parser_alloc(int int_n, Runtime runtime) {
	int* p = calloc(int_n, sizeof(int));
	set_insert(runtime->allocs, p);
	return p;
}

void parser_free(Pointer p, Runtime runtime) {
	set_remove(runtime->allocs, p);	// this does the free
}

// if token is of the form array[foo], it transforms it to array\0\foo\0 and returns foo
static String array_index(String token) {
	String bracket = strstr(token, "[");
	if(!bracket)
		return NULL;

	String index = bracket + 1;
	*bracket = '\0';
	index[strlen(index)-1] = '\0';
	return index;
}

static int* create_or_get_variable(String name, Runtime runtime) {
	int* variable = map_find(runtime->variables, name);
	if(variable == NULL) {
		variable = parser_alloc(1, runtime);
		*variable = *name >= '0' && *name <= '9' ? atoi(name) : 0;		// to create "constant variable"
		map_insert(runtime->variables, name, variable);
	}
	return variable;
}

static Array create_or_get_array(String name, int size, Runtime runtime) {
	Array array = map_find(runtime->arrays, name);
	if(array == NULL) {
		array = parser_alloc(1 + size, runtime);
		array[0] = size;	// size
		array++;		// point to the first element
		map_insert(runtime->arrays, name, array);
	}
	return array;
}

// add an argument to the instruction instr (if not null)
static void instr_add_arg(BCInstruction instr, int* arg) {
	if(arg)
		instr->args[instr->arg_n++] = arg;
}

static BCInstruction create_bc_instruction(Opcode opcode, int n, int* variable, Array array) {
	BCInstruction instr = calloc(1, sizeof(*instr));
	instr->opcode = opcode;
	instr->n = n;
	instr->arg_n = 0;
	instr_add_arg(instr, variable);
	instr_add_arg(instr, array);
	return instr;
}

static void create_load_varexpr(int reg, String token, Runtime runtime) {
	String index = array_index(token);

	if(index) {
		BCInstruction load_array = create_bc_instruction(0, -1, NULL, NULL);
		load_array->opcode = reg == 1 ? OP_LOAD1_A : OP_LOAD2_A;
		instr_add_arg(load_array, create_or_get_variable(index, runtime));
		instr_add_arg(load_array, create_or_get_array(token, 0, runtime));		// must be last
		vector_insert_last(runtime->code, load_array);

	} else {
		BCInstruction load_var = create_bc_instruction(reg == 1 ? OP_LOAD1_V : OP_LOAD2_V, -1, create_or_get_variable(token, runtime), NULL);
		vector_insert_last(runtime->code, load_var);
	}
}

static void create_store_varexpr(String token, Runtime runtime) {
	String index = array_index(token);

	if(*token >= '0' && *token <= '9') {
		printf("cannot store to constant %s\n", token);
		exit(-1);

	} else if(index) {
		BCInstruction store_array = create_bc_instruction(0, -1, NULL, NULL);
		store_array->opcode = OP_STORE_A;
		store_array->args[store_array->arg_n++] = create_or_get_variable(index, runtime);
		instr_add_arg(store_array, create_or_get_array(token, 0, runtime));	// must be last

		vector_insert_last(runtime->code, store_array);

	} else {
		BCInstruction store_var = create_bc_instruction(OP_STORE_V, -1, create_or_get_variable(token, runtime), NULL);
		vector_insert_last(runtime->code, store_var);
	}
}

static void instr_add_var_or_array(BCInstruction instr, String token, String index, Runtime runtime) {
	if(index) {
		instr_add_arg(instr, create_or_get_variable(index, runtime));
		instr_add_arg(instr, create_or_get_array(token, 0, runtime));
	} else {
		instr_add_arg(instr, create_or_get_variable(token, runtime));
	}
}

static String inverse_oper(String op) {
	return
		strcmp(op, "==") == 0 ? "!=" :
		strcmp(op, "!=") == 0 ? "==" :
		strcmp(op, ">=") == 0 ? "<"  :
		strcmp(op, ">" ) == 0 ? "<=" :
		strcmp(op, "<=") == 0 ? ">"  :
		strcmp(op, "<" ) == 0 ? ">=" :
		NULL;
}

static void create_expression(String x, String oper, String y, String target, Runtime runtime) {
	// operations without superinstructions
	if(oper[0] == '*' || oper[0] == '/' || oper[0] == '%') {
		create_load_varexpr(1, x, runtime);
		create_load_varexpr(2, y, runtime);

		vector_insert_last(runtime->code, create_bc_instruction(
			oper[0] == '*' ? OP_MUL :
			oper[0] == '/' ? OP_DIV :
			oper[0] == '%' ? OP_MOD : -1,
			-1, NULL, NULL
		));
		return;
	}

	String x_index = array_index(x);
	String y_index = array_index(y);

	// we swap x,y
	//  - for >,>= (implemented as <,<=)
	//  - for symmetric operatios, when only one of the two operands is an array
	//
	bool is_inequality = (oper[0] == '<' || oper[0] == '>');
	if(oper[0] == '>' || (!is_inequality && x_index && !y_index)) {
		// if only one is array, it should be y
		String temp = y;
		y = x;
		x = temp;
		y_index = x_index;
		x_index = NULL;
	}
	Opcode opcode =
		oper[0] == '+' ? OP_ADD_VVV :
		oper[0] == '-' ? OP_SUB_VVV :
		strcmp(oper, "==") == 0 ? OP_EQ_VV :
		strcmp(oper, "!=") == 0 ? OP_NEQ_VV :
		strcmp(oper, "<") == 0 || strcmp(oper, ">") == 0  ? OP_LT_VV :
		OP_LE_VV;

	// if the args are arrays, we advance the opcode to select the VA/AV/AA variants
	opcode += (x_index ? 1 : 0) + (y_index ? 1 : 0) + (is_inequality && y_index ? 1 : 0);

	BCInstruction add = create_bc_instruction(opcode, -1, NULL, NULL);
	vector_insert_last(runtime->code, add);
	instr_add_var_or_array(add, x, x_index, runtime);
	instr_add_var_or_array(add, y, y_index, runtime);

	if(oper[0] == '+' || oper[0] == '-') {
		String target_index = array_index(target);
		if(target_index)
			add->opcode += 3;		// target is array, _A?? opcodes are 3 positions after the V?? ones
		instr_add_var_or_array(add, target, target_index, runtime);
	}
}

// target = x
void create_assignment(String x, String target, Runtime runtime) {
		String x_index = array_index(x);
		String target_index = array_index(target);
		Opcode opcode =
				OP_ASSIGN_VV + (x_index ? 1 : 0) + (target_index ? 2 : 0);

		BCInstruction assign = create_bc_instruction(opcode, -1, NULL, NULL);
		vector_insert_last(runtime->code, assign);
		instr_add_var_or_array(assign, x, x_index, runtime);
		instr_add_var_or_array(assign, target, target_index, runtime);
}

static int find_type(String tokens[], int token_n) {
	return
		strcmp(tokens[0], "write") == 0 ? WRITE :
		strcmp(tokens[0], "writeln") == 0 ? WRITELN :
		strcmp(tokens[0], "read") == 0 ? READ:
		token_n == 3 && strcmp(tokens[1], "=") == 0 ? ASSIGN_VAR :
		token_n == 5 && strcmp(tokens[1], "=") == 0 ? ASSIGN_EXP :
		strcmp(tokens[0], "if") == 0 ? IF :
		strcmp(tokens[0], "while") == 0 ? WHILE :
		strcmp(tokens[0], "random") == 0 ? RAND :
		strcmp(tokens[0], "argument") == 0 && strcmp(tokens[1], "size") == 0 ? ARG_SIZE :
		strcmp(tokens[0], "argument") == 0 ? ARG :
		strcmp(tokens[0], "break") == 0 ? BREAK :
		strcmp(tokens[0], "continue") == 0 ? CONTINUE :
		strcmp(tokens[0], "new") == 0 ? NEW :
		strcmp(tokens[0], "free") == 0 ? FREE :
		strcmp(tokens[0], "size") == 0 ? SIZE :
		-1;
}

static void generate_program_code(Program prog, Runtime runtime);

static void generate_statement_code(Statement stm, Runtime runtime) {
	stm->start_pos = vector_size(runtime->code);
	String tok0 = stm->tokens[0];
	String tok1 = stm->tokens[1];
	String tok2 = stm->tokens[2];
	String tok3 = stm->tokens[3];
	String tok4 = stm->tokens[4];

	switch(stm->type) {
		case WRITE:
		case WRITELN:
			create_load_varexpr(1, tok1, runtime);
			vector_insert_last(runtime->code, create_bc_instruction(stm->type == WRITE ? OP_WRITE : OP_WRITELN, -1, NULL, NULL));
			break;

		case READ:
		case RAND:
			vector_insert_last(runtime->code, create_bc_instruction(stm->type == READ ? OP_READ : OP_RAND, -1, NULL, NULL));
			create_store_varexpr(tok1, runtime);
			break;

		case ASSIGN_VAR:
			//  create_load_varexpr(1, tok2, runtime);
			//  create_store_varexpr(tok0, runtime);
			// tok3 = "+";
			// tok4 = "0";
			create_assignment(tok2, tok0, runtime);
			break;

		case ASSIGN_EXP:
			// E = E +/- 1  and   E = 1 +/- E    are implemented via INC/DEC
			if((tok3[0] == '+' || tok3[0] == '-') &&
			   ((strcmp(tok0, tok2) == 0 && strcmp(tok4, "1") == 0) || (strcmp(tok0, tok4) == 0 && strcmp(tok2, "1") == 0))) {
				String bracket = strstr(tok0, "[");
				String index = bracket + 1;
				if(bracket) {
					*bracket = '\0';
					index[strlen(index)-1] = '\0';
				}
				int* array = bracket ? create_or_get_array(tok0, 0, runtime) : NULL;
				int* var = create_or_get_variable(bracket ? index : tok0, runtime);
				int opcode = tok3[0] == '+' 
					? (bracket ? OP_INC_A : OP_INC_V)
					: (bracket ? OP_DEC_A : OP_DEC_V);
				vector_insert_last(runtime->code, create_bc_instruction(opcode, -1, var, array));

			} else {
				create_expression(tok2, tok3, tok4, tok0, runtime);
				// ADD/SUB opcodes include the assignment
				if(tok3[0] != '+' && tok3[0] != '-')
					create_store_varexpr(tok0, runtime);
			}
			break;

		case IF:
		case WHILE: {
			// the jump offset will be filled after generating the body code
			BCInstruction jump_over_body = NULL;

			bool always_true = strcmp(tok1, tok3) == 0 && strcmp(tok2, "==") == 0;
			if(!always_true) {
				// test instrutions (eg OP_EQ_VV) do a test&jump, no separate jump is needed!
				create_expression(tok1, tok2, tok3, NULL, runtime);
				jump_over_body = vector_get_at(runtime->code, vector_size(runtime->code)-1);  // last instr is the test&jump
			}

			// generate the body code
			int guard_length = vector_size(runtime->code) - stm->start_pos;
			generate_program_code(stm->body, runtime);

 			// if we have a WHILE, a jump back to start should be added at the end of the body
			BCInstruction jump_back_to_start = NULL;
			if(stm->type == WHILE) {
				if(always_true) {
					// infinite loop, we do an unconditional jump back
					jump_back_to_start = create_bc_instruction(OP_JUMP, -1, NULL, NULL);
					vector_insert_last(runtime->code, jump_back_to_start);
				} else {
					// normal loop with a test
					// __Optimization__: instead of jumping back to start, and then
					// do the while's test, we do the inverse test here, and jump to the start
					// of the code. So we do a single jump, not two.
					// Essential we transform
					//    while(cond} { ...  }
					// to
					//    if(cond) { do { ... } while(code) }
					create_expression(tok1, inverse_oper(tok2), tok3, NULL, runtime);
					jump_back_to_start = vector_get_at(runtime->code, vector_size(runtime->code)-1);
				}
			}

			// if we have an else we need to jump over it at the end of body
			BCInstruction jump_over_else = NULL;
			if(stm->else_body) {
				jump_over_else = create_bc_instruction(OP_JUMP, -1, NULL, NULL);
				vector_insert_last(runtime->code, jump_over_else);
			}

			// now we can jump over the body
			int body_length = vector_size(runtime->code) - stm->start_pos - guard_length;
			if(jump_over_body != NULL)
				jump_over_body->n = body_length;

			// for WHILE, we need to jump back to the start of __code__ (not the start of while),
			// see the __optimization__ above
			if(stm->type == WHILE)
				jump_back_to_start->n = -body_length;

			// add the else code, if it exists
			if(stm->else_body) {
				generate_program_code(stm->else_body, runtime);

				// at the end of the body we jump over else
				int else_length = vector_size(runtime->code) - stm->start_pos - body_length - guard_length;
				jump_over_else->n = else_length;
			}
			break;
		}

		case BREAK:
		case CONTINUE:
			// the exact jump will be filled later
			vector_insert_last(runtime->code, create_bc_instruction(OP_JUMP, -1, NULL, NULL));
			break;

		case NEW: {
			Array array = create_or_get_array(strtok(tok1, "[]"), 0, runtime);
			create_load_varexpr(1, strtok(NULL, "[]"), runtime);
			vector_insert_last(runtime->code, create_bc_instruction(OP_NEW, -1, NULL, array));
			break;
		}

		case FREE: {
			Array array = create_or_get_array(tok1, 0, runtime);
			vector_insert_last(runtime->code, create_bc_instruction(OP_FREE, -1, NULL, array));
			break;
		}

		case SIZE:
		case ARG_SIZE: {
			Array array = create_or_get_array(stm->type == SIZE ? tok1 : "!args", 0, runtime);
			vector_insert_last(runtime->code, create_bc_instruction(OP_SIZE, -1, NULL, array));
			create_store_varexpr(tok2, runtime);
			break;
		}

		case ARG: {
			// create_load_varexpr(1, tok1, runtime);

			BCInstruction load_array = create_bc_instruction(OP_LOAD1_A, -1, create_or_get_variable(tok1, runtime), create_or_get_array("!args", 0, runtime));
			vector_insert_last(runtime->code, load_array);

			BCInstruction store_var = create_bc_instruction(OP_STORE_V, -1, create_or_get_variable(tok2, runtime), NULL);
			vector_insert_last(runtime->code, store_var);
			break;
		}
	}

	stm->end_pos = vector_size(runtime->code);
}

static void generate_program_code(Program prog, Runtime runtime) {
	for(int i = 0; i < vector_size(prog); i++)
		generate_statement_code(vector_get_at(prog, i), runtime);
}

static void set_break_continue_offsets(Program prog, Runtime runtime, Vector while_stack) {
	for(int i = 0; i < vector_size(prog); i++) {
		Statement stm = vector_get_at(prog, i);

		if(stm->type == BREAK || stm->type == CONTINUE) {
			BCInstruction jump = vector_get_at(runtime->code, stm->start_pos);
			int levels = stm->tokens[1] ? atoi(stm->tokens[1]) : 1;
			if(levels > vector_size(while_stack)) {
				printf("invalid break/continue\n");
				exit(-1);
			}
			Statement while_stm = vector_get_at(while_stack, vector_size(while_stack) - levels);
			jump->n = (stm->type == BREAK ? while_stm->end_pos : while_stm->start_pos) - (stm->start_pos + 1);	// +1 cause the IP is after the break
		}

		if(stm->type == WHILE)
			vector_insert_last(while_stack, stm);

		if(stm->body)
			set_break_continue_offsets(stm->body, runtime, while_stack);
		if(stm->else_body)
			set_break_continue_offsets(stm->else_body, runtime, while_stack);

		if(stm->type == WHILE)
			vector_remove_last(while_stack);
	}
}

static Vector get_nested_source(Vector source, int start) {
	Vector nested = vector_create(0, NULL);
	for(int i = start; i < vector_size(source); i++) {
		String line = vector_get_at(source, i);
		if(line[0] != '\t' && line[0] != '\0' && line[0] != '#' && line[0] != '\n')
			break;
		vector_insert_last(nested, line + (line[0] == '\t' ? 1 : 0));	// skip initial tab
	}
	return nested;
}

static void destroy_statement(Pointer p) {
	Statement stm = p;
	if(stm->body)
		vector_destroy(stm->body);
	if(stm->else_body)
		vector_destroy(stm->else_body);
	free(p);
}

static Program parse(Vector source, Runtime runtime) {
	Program prog = vector_create(0, destroy_statement);

	for(int i = 0; i < vector_size(source); i++) {
		String line = vector_get_at(source, i);

		// split in tokens
		String tokens[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
		int token_n = 0;
		for(String token = strtok(line, " \t\n"); token_n < 6 && token != NULL && token[0] != '#'; token = strtok(NULL, " \t\n"))
			tokens[token_n++] = token;

		if(token_n == 0)
			continue;		// empty line

		// else branches should be inserted in the last if statement
		if(strcmp(tokens[0], "else") == 0) {
			Vector nested = get_nested_source(source, i+1);
			Statement stm = vector_get_at(prog, vector_size(prog) - 1);
			stm->else_body = parse(nested, runtime);
			i += vector_size(nested);
			vector_destroy(nested);
			continue;
		}

		int type = find_type(tokens, token_n);
		if(type == -1) {
			printf("error in line %s\n", line);
			exit(1);
		}
		Statement stm = calloc(1, sizeof(*stm));
		stm->type = type;
		memcpy(stm->tokens, tokens, 5*sizeof(String));
		vector_insert_last(prog, stm);

		// if/while have bodies
		if(stm->type == IF || stm->type == WHILE) {
			Vector nested = get_nested_source(source, i+1);
			stm->body = parse(nested, runtime);
			i += vector_size(nested);
			vector_destroy(nested);
		}
	}

	return prog;
}

static int compare_pointers(Pointer a, Pointer b) {
	return a - b;
}

Runtime parser_create_runtime(Vector source, Vector args, bool verbose) {
	Runtime runtime = calloc(1, sizeof(*runtime));
	runtime->variables = map_create((CompareFunc)strcmp, NULL, NULL);
	runtime->arrays = map_create((CompareFunc)strcmp, NULL, NULL);
	runtime->allocs = set_create(compare_pointers, free);
	runtime->verbose = verbose;

	// create "!args" array containing all arguments
	Array args_arr = create_or_get_array("!args", vector_size(args)+1, runtime);
	args_arr[-1] = vector_size(args);
	for(int i = 0; i < args_arr[-1]; i++)
		args_arr[i+1] = atoi(vector_get_at(args, i)); // i+1 cause argument command is 1-based

	// parse
	Program program = parse(source, runtime);

	// generate bytecode
	runtime->code = vector_create(0, free);
	generate_program_code(program, runtime);
	vector_insert_last(runtime->code, create_bc_instruction(OP_HALT, -1, NULL, NULL));

	// setup break/contunue
	Vector while_stack = vector_create(0, NULL);
	set_break_continue_offsets(program, runtime, while_stack);
	vector_destroy(while_stack);

	// no longer needed
	map_destroy(runtime->variables);
	map_destroy(runtime->arrays);
	vector_destroy(program);
	runtime->variables = runtime->arrays = NULL;

	return runtime;
}

void parser_destroy_runtime(Runtime runtime) {
	set_destroy(runtime->allocs);
	vector_destroy(runtime->code);
	free(runtime);
}
