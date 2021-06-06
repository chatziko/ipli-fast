
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "ADTMap.h"
#include "interpreter.h"

// if PROFILE is defined, count how many times each instruction is executed
// #define PROFILE

#ifdef PROFILE
#define INC_COUNTER ((BCInstruction)map_find(thread_to_instr, ip))->exec_count++;
#else
#define INC_COUNTER
#endif

#define NEXT INC_COUNTER goto **ip++;


bool is_jump(BCInstruction instr) {
	return
		instr->opcode == OP_JUMP ||
		(instr->opcode >= OP_EQ_VV && instr->opcode <= OP_LT_AA);
}

void print_code(Vector code) {
	String opcodes[] = {
		"OP_WRITE", "OP_WRITELN", "OP_READ",
		"OP_LOAD1_V", "OP_LOAD1_A",
		"OP_LOAD2_V", "OP_LOAD2_A",
		"OP_STORE_V", "OP_STORE_A",
		"OP_ASSIGN_VV", "OP_ASSIGN_VA", "OP_ASSIGN_AV", "OP_ASSIGN_AA",
		"OP_INC_V", "OP_INC_A", "OP_DEC_V", "OP_DEC_A",
		"OP_JUMP", "OP_RAND", "OP_NEW", "OP_FREE", "OP_SIZE", "OP_HALT",
		"OP_ADD_VVV", "OP_ADD_VVA", "OP_ADD_VAA", "OP_ADD_AVV", "OP_ADD_AVA", "OP_ADD_AAA",
		"OP_SUB_VVV", "OP_SUB_VVA", "OP_SUB_VAA", "OP_SUB_AVV", "OP_SUB_AVA", "OP_SUB_AAA",
		"OP_MUL", "OP_DIV", "OP_MOD",
		"OP_EQ_VV", "OP_EQ_VA", "OP_EQ_AA", "OP_NEQ_VV", "OP_NEQ_VA", "OP_NEQ_AA",
		"OP_LE_VV", "OP_LE_VA", "OP_LE_AV", "OP_LE_AV", "OP_LT_VV", "OP_LT_VA", "OP_LT_AV", "OP_LT_AA",
	};

	for(int i = 0; i < vector_size(code); i++) {
		BCInstruction instr = vector_get_at(code, i);
		printf("%-12s (%d)", opcodes[instr->opcode], instr->exec_count);
		if(is_jump(instr))
			printf(" %d", instr->n);
		for(int i = 0; i < instr->arg_n; i++)
			printf(" %p", instr->args[i]);
		printf("\n");
	}
}

int compare_pointers(Pointer a, Pointer b) {
	return a - b;
}

void interpreter_run(Runtime runtime) {
	if(runtime->verbose)
		print_code(runtime->code);

	void* labels[] = {
		&&OP_WRITE, &&OP_WRITELN, &&OP_READ,
		&&OP_LOAD1_V, &&OP_LOAD1_A,
		&&OP_LOAD2_V, &&OP_LOAD2_A,
		&&OP_STORE_V, &&OP_STORE_A,
		&&OP_ASSIGN_VV, &&OP_ASSIGN_VA, &&OP_ASSIGN_AV, &&OP_ASSIGN_AA, 
		&&OP_INC_V, &&OP_INC_A, &&OP_DEC_V, &&OP_DEC_A,
		&&OP_JUMP, &&OP_RAND, &&OP_NEW, &&OP_FREE, &&OP_SIZE, &&OP_HALT,
		&&OP_ADD_VVV, &&OP_ADD_VVA, &&OP_ADD_VAA, &&OP_ADD_AVV, &&OP_ADD_AVA, &&OP_ADD_AAA,
		&&OP_SUB_VVV, &&OP_SUB_VVA, &&OP_SUB_VAA, &&OP_SUB_AVV, &&OP_SUB_AVA, &&OP_SUB_AAA,
		&&OP_MUL, &&OP_DIV, &&OP_MOD,
		&&OP_EQ_VV, &&OP_EQ_VA, &&OP_EQ_AA, &&OP_NEQ_VV, &&OP_NEQ_VA, &&OP_NEQ_AA,
		&&OP_LE_VV, &&OP_LE_VA, &&OP_LE_AV, &&OP_LE_AA, &&OP_LT_VV, &&OP_LT_VA, &&OP_LT_AV, &&OP_LT_AA,
	};

	// find thread size
	int instr_n = vector_size(runtime->code);
	int thread_n = 0;
	for(int i = 0; i < instr_n; i++) {
		BCInstruction instr = vector_get_at(runtime->code, i);
		thread_n += 1 + instr->arg_n + (is_jump(instr) ? 1 : 0);
	}

	// setup thread
	void** thread = malloc(thread_n * sizeof(*thread));
	void** t = thread;

	#ifdef PROFILE
	// map thread locations to instructions, if profiling
	Map thread_to_instr = map_create(compare_pointers, NULL, NULL);
	#endif

	for(int i = 0; i < instr_n; i++) {
		BCInstruction instr = vector_get_at(runtime->code, i);

		instr->thread_pos = t;
		*t++ = labels[instr->opcode];

		// leave space for the target thread address, we'll fill it after we know the position of all instructions
		if(is_jump(instr))
			t++;
		
		for(int j = 0; j < instr->arg_n; j++)
			*t++ = instr->args[j];

		#ifdef PROFILE
		map_insert(thread_to_instr, instr->thread_pos, instr);
		#endif
	}
	assert(t - thread == thread_n);

	// setup jumps, now that we know the location of all instructions in the thread
	for(int i = 0; i < instr_n; i++) {
		BCInstruction instr = vector_get_at(runtime->code, i);

		if(is_jump(instr)) {
			BCInstruction target = vector_get_at(runtime->code, i + 1 + instr->n);
			*(instr->thread_pos+1) = target->thread_pos;
		}
	}

	register int reg1 = 0;
	register int reg2 = 0;
	register void** ip = thread;		// pointer to _next_ instruction
	NEXT						// gcc syntax, we dereference a void* to jump to that location

	OP_LOAD1_V:
		reg1 = *(int*)*ip++;
		NEXT

	OP_LOAD2_V:
		reg2 = *(int*)*ip++;
		NEXT

	OP_LOAD1_A:
		reg1 = ((Array)*(ip+1))[*(int*)*ip];
		ip += 2;
		NEXT

	OP_LOAD2_A:
		reg2 = ((Array)*(ip+1))[*(int*)*ip];
		ip += 2;
		NEXT

	OP_STORE_V:
		*(int*)*ip++ = reg1;
		NEXT

	OP_STORE_A:
		((Array)*(ip+1))[*(int*)*ip] = reg1;
		ip += 2;
		NEXT

	// assign ///////////////////////
	OP_ASSIGN_VV:
		*(int*)*(ip+1) = *(int*)*ip;
		ip += 2;
		NEXT

	OP_ASSIGN_VA:
		*(int*)*(ip+2) = ((Array)*(ip+1))[*(int*)*ip];
		ip += 3;
		NEXT

	OP_ASSIGN_AV:
		((Array)*(ip+2))[*(int*)*(ip+1)] = *(int*)*ip;
		ip += 3;
		NEXT

	OP_ASSIGN_AA:
		((Array)*(ip+3))[*(int*)*(ip+2)] = ((Array)*(ip+1))[*(int*)*ip];
		ip += 4;
		NEXT


	OP_INC_V:
		++ *(int*)(*ip++);
		NEXT

	OP_INC_A:
		++ ((Array)*(ip+1))[*(int*)*ip];
		ip += 2;
		NEXT

	OP_DEC_V:
		-- *(int*)(*ip++);
		NEXT

	OP_DEC_A:
		-- ((Array)*(ip+1))[*(int*)*ip];
		ip += 2;
		NEXT

	OP_JUMP:
		ip = *ip;
		NEXT

	// ADD /////////////////////////////

	OP_ADD_VVV:
		*(int*)*(ip+2) = *(int*)*ip + *(int*)*(ip+1);
		ip += 3;
		NEXT

	OP_ADD_VVA:
		*(int*)*(ip+3) = *(int*)*ip + ((Array)*(ip+2))[*(int*)*(ip+1)];
		ip += 4;
		NEXT

	OP_ADD_VAA:
		*(int*)*(ip+4) = ((Array)*(ip+1))[*(int*)*ip] + ((Array)*(ip+3))[*(int*)*(ip+2)];
		ip += 5;
		NEXT

	OP_ADD_AVV:
		((Array)*(ip+3))[*(int*)*(ip+2)] = *(int*)*ip + *(int*)*(ip+1);
		ip += 4;
		NEXT

	OP_ADD_AVA:
		((Array)*(ip+4))[*(int*)*(ip+3)] = *(int*)*ip + ((Array)*(ip+2))[*(int*)*(ip+1)];
		ip += 5;
		NEXT

	OP_ADD_AAA:
		((Array)*(ip+5))[*(int*)*(ip+4)] = ((Array)*(ip+1))[*(int*)*ip] + ((Array)*(ip+3))[*(int*)*(ip+2)];
		ip += 6;
		NEXT

	// SUB /////////////////////////////

	OP_SUB_VVV:
		*(int*)*(ip+2) = *(int*)*ip - *(int*)*(ip+1);
		ip += 3;
		NEXT

	OP_SUB_VVA:
		*(int*)*(ip+3) = *(int*)*ip - ((Array)*(ip+2))[*(int*)*(ip+1)];
		ip += 4;
		NEXT

	OP_SUB_VAA:
		*(int*)*(ip+4) = ((Array)*(ip+1))[*(int*)*ip] - ((Array)*(ip+3))[*(int*)*(ip+2)];
		ip += 5;
		NEXT

	OP_SUB_AVV:
		((Array)*(ip+3))[*(int*)*(ip+2)] = *(int*)*ip - *(int*)*(ip+1);
		ip += 4;
		NEXT

	OP_SUB_AVA:
		((Array)*(ip+4))[*(int*)*(ip+3)] = *(int*)*ip - ((Array)*(ip+2))[*(int*)*(ip+1)];
		ip += 5;
		NEXT

	OP_SUB_AAA:
		((Array)*(ip+5))[*(int*)*(ip+4)] = ((Array)*(ip+1))[*(int*)*ip] - ((Array)*(ip+3))[*(int*)*(ip+2)];
		ip += 6;
		NEXT

	/////////////////////

	OP_EQ_VV:
		ip = *(int*)*(ip+1) == *(int*)*(ip+2)
			? ip+3 : *ip;
		NEXT

	OP_EQ_VA:
		ip = *(int*)*(ip+1) == ((Array)*(ip+3))[*(int*)*(ip+2)]
			? ip+4 : *ip;
		NEXT

	OP_EQ_AA:
		ip = ((Array)*(ip+2))[*(int*)*(ip+1)] == ((Array)*(ip+4))[*(int*)*(ip+3)]
			? ip+5 : *ip;
		NEXT

	OP_NEQ_VV:
		ip = *(int*)*(ip+1) != *(int*)*(ip+2)
			? ip+3 : *ip;
		NEXT

	OP_NEQ_VA:
		ip = *(int*)*(ip+1) != ((Array)*(ip+3))[*(int*)*(ip+2)]
			? ip+4 : *ip;
		NEXT

	OP_NEQ_AA:
		ip = ((Array)*(ip+2))[*(int*)*(ip+1)] != ((Array)*(ip+4))[*(int*)*(ip+3)]
			? ip+5 : *ip;
		NEXT

	OP_LE_VV:
		ip = *(int*)*(ip+1) <= *(int*)*(ip+2)
			? ip+3 : *ip;
		NEXT

	OP_LE_VA:
		ip = *(int*)*(ip+1) <= ((Array)*(ip+3))[*(int*)*(ip+2)]
			? ip+4 : *ip;
		NEXT

	OP_LE_AV:
		ip = ((Array)*(ip+2))[*(int*)*(ip+1)] <= *(int*)*(ip+3)
			? ip+4 : *ip;
		NEXT

	OP_LE_AA:
		ip = ((Array)*(ip+2))[*(int*)*(ip+1)] <= ((Array)*(ip+4))[*(int*)*(ip+3)]
			? ip+5 : *ip;
		NEXT

	OP_LT_VV:
		ip = *(int*)*(ip+1) < *(int*)*(ip+2)
			? ip+3 : *ip;
		NEXT

	OP_LT_VA:
		ip = *(int*)*(ip+1) < ((Array)*(ip+3))[*(int*)*(ip+2)]
			? ip+4 : *ip;
		NEXT

	OP_LT_AV:
		ip = ((Array)*(ip+2))[*(int*)*(ip+1)] < *(int*)*(ip+3)
			? ip+4 : *ip;
		NEXT

	OP_LT_AA:
		ip = ((Array)*(ip+2))[*(int*)*(ip+1)] < ((Array)*(ip+4))[*(int*)*(ip+3)]
			? ip+5 : *ip;
		NEXT


	//////////////////////////////

	OP_MUL:
		reg1 = reg1 * reg2;
		NEXT

	OP_DIV:
		reg1 = reg1 / reg2;
		NEXT

	OP_MOD:
		reg1 = reg1 % reg2;
		NEXT

	OP_NEW: {
		int* old_array = *ip;
		parser_free(old_array - 1, runtime);	// we always have a placeholder memory reserved
		int* new_array = parser_alloc(reg1+1, runtime);
		new_array[0] = reg1;				// we store the size in the first element
		new_array++;						// and point to the second element
		for(int i = 0; i < thread_n; i++)	// replace all occurrences of old_array in the thread table
			if(thread[i] == old_array)
				thread[i] = new_array;
		ip++;
		NEXT
	}

	OP_FREE: {
		int* old_array = *ip;
		parser_free(old_array - 1, runtime);
		int* new_array = parser_alloc(1, runtime); // we create a placeholder empty array
		new_array[0] = 0;					// we store the size in the first element
		new_array++;						// and point to the second element
		for(int i = 0; i < thread_n; i++)	// replace all occurrences of old_array in the thread table
			if(thread[i] == old_array)
				thread[i] = new_array;
		ip++;
		NEXT
	}

	OP_SIZE:
		reg1 = ((Array)*ip++)[-1];
		NEXT

	OP_WRITE:
		printf("%d ", reg1);
		NEXT

	OP_WRITELN:
		printf("%d\n", reg1);
		NEXT

	OP_READ: {
		int temp;
		if(!scanf("%d", &temp))
			return;
		reg1 = temp;
		NEXT
	}

	OP_RAND:
		reg1 = rand();
		NEXT

	OP_HALT:
		#ifdef PROFILE
		print_code(runtime->code);
		map_destroy(thread_to_instr);
		#endif
		free(thread);
		return;
}