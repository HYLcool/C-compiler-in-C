#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>


// basic data
int token; // current token
char *src,*old_src; // pointer to source code string
int poolSize; // default size of text/data/stack
int line; // line number

// segment data
int *text; // text segment
int *old_text; // for dump text segment
int *stack; // stack
char * data; // data segment

// register data
int *pc; // pc register, storing the address of the next command
int *sp; // sp register, always pointing to the top of stack
int *bp; // bp register, pointing to the stack, used when calling a function
int ax; // ax register, storing the last result of command
int cycle;

// identifier data
int token_val; // value of current token (mainly for number)
int *currnet_id; // current parsed ID
int *symbols; // symbol table

// global declaration data
int basetype; // the type of a declaration, make it global for convenience
int expr_type; // the type of an expression

// index of bp pointer on stack
int index_of_bp;

// instructions
enum { LEA, IMM, JMP, CALL, JZ, JNZ, ENT, ADJ, LEV, LI, LC, SI, SC, PUSH, OR, XOR, AND,
	EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, OPEN, READ, CLOS, PRTF,
	MALC, MSET, MCMP, EXIT};

// tokens and classes (operators last and in precedence order)
enum {Num = 128, Fun, Sys, Glo, Loc, Id, Char, Else, Enum, If, Int, Return, Sizeof, While,
	Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul,
	Div, Mod, Inc, Dec, Brak};

// field of identifier
enum
{
	Token, // token of identifier
	Hash, // hash value of identifier
	Name, // name of identifier
	Type, // type of identifier, number, local ...
	Class, // class of identifier, int, char, ...
	Value, // value of identifier
	Btype, // if one local identifier and one global identifier have the same name, information of global one is stored here
	Bclass,
	Bvalue,
	IdSize
};

// types of variable/function
enum {CHAR ,INT, PTR};

int *idmain; // main function

// get the next token, it will ignore spaces automatically
void next()
{
	char *last_pos;
	int hash;

	while(token = *src)
	{
		++src;

		// parse token

		if(token == '\n') // a new line
			++line;
		else if(token == '#') // skip macro, because we will not support it
		{
			while(*src != 0 && *src !='\n')
				++src;
		}
		else if((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || token == '_') // parse identifier
		{
			last_pos = src - 1;
			hash = token;

			while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || *src == '_')
			{
				hash = hash * 147 + *src;
				src++;
			}

			// look for existing identifier on linear search
			currnet_id = symbols;
			while(currnet_id[Token])
			{
				if(currnet_id[Hash] == hash && !memcmp((char*)currnet_id[Name], last_pos, src - last_pos)) // find one, return
				{
					token = currnet_id[Token];
					return;
				}
				currnet_id = currnet_id + IdSize;
			}

			// didn't find, so it's a new one, store it in array
			currnet_id[Name] = (int)last_pos;
			currnet_id[Hash] = hash;
			token = currnet_id[Token] = Id;
			return;
		}
		else if(token >= '0' && token <= '9') // parse numbers of 3 kinds: dec(123), hex(0x123), oct(0123)
		{
			token_val = token - '0';
			if(token_val)
			{
				if(*src == 'x' || *src == 'X') // hex
				{
					token = *++src;
					while((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F'))
					{
						token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
						token = *++src;
					}
				}
				else
				{
					while(*src >= '0' && *src <= '9')
					{
						token_val = token_val * 10 + *src++ - '0';
					}
				}
			}
			else
			{
				while(*src >= '0' && *src <= '7')
				{
					token_val = token_val * 8 + *src++ - '0';
				}
			}

			token = Num;
			return;
		}
		else if(token == '"' || token == '\'') // parse string literal, currently, the only supported escape chracter is '\n', store the string literal into data segment
		{
			last_pos = data;
			while(*src != 0 && *src != token)
			{
				token_val = *src++;
				if(token_val == '\\')
				{
					token_val = *src++;
					if(token_val == 'n')
						token_val = '\n';
				}
				if(token == '"')
					*data++ = token_val;
			}

			++src;

			// single character, return as a Num
			if(token == '"')
				token_val = (int)last_pos;
			else
				token = Num;

			return;
		}
		else if(token == '/') // comments, only support // type, not /*...*/
		{
			if(token == '/')
			{
				while(*src != 0 && *src != '\n') // skip comments
					++src;
			}
			else // it's divide operator
			{
				token = Div;
				return;
			}
		}
		else if(token == '=')
		{
			if(*src == '=') // it's ==
			{
				++src;
				token = Eq;
			}
			else // it's =
				token = Assign;
			return;
		}
		else if(token == '+')
		{
			if(*src == '+') // it's ++
			{
				++src;
				token = Inc;
			}
			else // it's +
				token = Add;
			return;
		}
		else if(token == '-')
		{
			if(*src == '-') // it's --
			{
				++src;
				token = Dec;
			}
			else // it's -
				token = Sub;
			return;
		}
		else if(token == '!') // it's !=
		{
			if(*src == '=')
			{
				++src;
				token = Ne;
			}
			return;
		}
		else if(token == '<')
		{
			if(*src == '=') // it's <=
			{
				++src;
				token = Le;
			}
			else if(*src == '<') // it's <<
			{
				++src;
				token = Shl;
			}
			else // it's <
				token = Lt;
			return;
		}
		else if(token == '>')
		{
			if(*src == '=') // it's >=
			{
				++src;
				token = Ge;
			}
			else if(*src == '>') // it's >>
			{
				++src;
				token = Shr;
			}
			else // it's >
				token = Gt;
			return;
		}
		else if(token == '|')
		{
			if(*src == '|')
			{
				++src;
				token = Lor;
			}
			else
				token = Or;
			return;
		}
		else if(token == '&')
		{
			if(*src == '&') // it's &&
			{
				++src;
				token = Lan;
			}
			else // it's &
				token = And;
		}
		else if(token == '^')
		{
			token = Xor;
			return;
		}
		else if(token == '%')
		{
			token = Mod;
			return;
		}
		else if(token == '*')
		{
			token = Mul;
			return;
		}
		else if(token == '[')
		{
			token = Brak;
			return;
		}
		else if(token == '?')
		{
			token = Cond;
			return;
		}
		else if(token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':')
		{
			// can return as token directly
			return;
		}
	}
}	

// a package of next(), if it's not prospective, print the error inf
void match(int tk)
{
	if(token == tk)
		next();
	else
	{
		printf("%d: expected token: %d\n", line, tk);
		exit(-1);
	}
}

// declaration of enum type
// parse enum[id]{a=10,b=20,c,d,...}
void enum_declaration()
{
	int i;
	i = 0;
	while(token != '}')
	{
		if(token != Id)
		{
			printf("%d: bad enum identifier %d\n", line, token);
			exit(-1);
		}
		next();
		if(token == Assign) // like {a=10,...}
		{
			next();
			if(token != Num)
			{
				printf("%d: bad enum initializer\n", line);
				exit(-1);
			}
			i = token_val;
			next();
		}

		currnet_id[Class] = Num;
		currnet_id[Type] = INT;
		currnet_id[Value] = i++;

		if(token == ',')
			next();
	}
}

// analyse the parameters
void function_parameter()
{
	int type;
	int params;
	params = 0;
	while(token != ')') // similar to analysing the variables
	{
		type = INT;
		if(token == Int)
			match(Int);
		else if(token == Char)
		{
			type = CHAR;
			match(Char);
		}

		// pointer
		while(token == Mul)
		{
			match(Mul);
			type = type + PTR;
		}

		// parameter name
		if(token != Id)
		{
			printf("%d: bad parameter declaration\n", line);
			exit(-1);
		}
		if(currnet_id[Class] == Loc)
		{
			printf("%d: duplicate parameter declaration\n", line);
			exit(-1);
		}

		match(Id);

		// store the local variables
		currnet_id[Bclass] = currnet_id[Class];
		currnet_id[Class] = Loc;
		currnet_id[Btype] = currnet_id[Type];
		currnet_id[type] = type;
		currnet_id[Bvalue] = currnet_id[Value];
		currnet_id[Value] = params++;

		if(token == ',')
			match(',');
	}

	index_of_bp = params + 1;
}

// analyse a expression
void expression(int level)
{
	int *id, tmp;
	int *addr;
	if(!token)
	{
		printf("%d: unexpected token EOF of expression\n", line);
		exit(-1);
	}
	if(token == Num)
	{
		match(Num);

		// emit code
		*++text = IMM;
		*++text = token_val;
		expr_type = INT;
	}
	else if(token == '"')
	{
		*++text = IMM;
		*++text = token_val;

		match('"');

		// store the rest strings
		while(token == '"')
			match('"');

		// append the end of string character '\0', all the data are default to 0, so just move data one position forward.
		data = (char*)(((int)data + sizeof(int)) & (-sizeof(int)));
		expr_type = PTR;
	}
	else if(token == Sizeof)
	{
		// sizeof is actually a unary operator, now only 'sizeof(int)', 'sizeof(char)', 'sizeof(*...)' are supported
		match(Sizeof);
		match('(');
		expr_type = INT;

		if(token == Int)
			match(Int);
		else if(token == Char)
		{
			match(Char);
			expr_type = CHAR;
		}

		while(token == Mul)
		{
			match(Mul);
			expr_type = expr_type + PTR;
		}

		match(')');

		*++text = IMM;
		*++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);
		expr_type = INT;
	}
	else if(token == Id)
	{
		// there are several types when occurs to Id, but this is unit, so it can only be
		// 1. fucntion call
		// 2. enum variable
		// 3. global/local variable

		match(Id);

		id = currnet_id;

		if(token == '(') // function call
		{
			match('(');

			// pass in arguments
			tmp = 0; // number of arguments
			while(token != ')')
			{
				expression(Assign);
				*++text = PUSH;
				++tmp;

				if(token == ',')
					match(',');
			}

			match(')');

			if(id[Class] == Sys) // system function
				*++text = id[Value];
			else if(id[Class] == Fun)
			{
				*++text = CALL;
				*++text = id[Value];
			}
			else
			{
				printf("%d: bad function call\n", line);
				exit(-1);
			}

			// clean the stack for arguments
			if(tmp > 0)
			{
				*++text = ADJ;
				*++text = tmp;
			}
			expr_type = id[Type];
		}
		else if(id[Class] == Num) // enum variable
		{
			*++text = IMM;
			*++text = id[Value];
			expr_type = INT;
		}
		else // variable
		{
			if(id[Class] == Loc)
			{
				*++text = LEA;
				*++text = index_of_bp - id[Value];
			}
			else if(id[Class] == Glo)
			{
				*++text = IMM;
				*++text = id[Value];
			}
			else
			{
				printf("%d: undefined variable\n", line);
				exit(-1);
			}

			// default behavior is to load the value of the address which is stored in ax
			expr_type = id[Type];
			*++text = (expr_type == Char) ? LC : LI;
		}
	}
	else if(token == '(') // cast or parenthesis
	{
		match('(');
		if(token == Int || token == Char)
		{
			tmp = (token == Char) ? CHAR : INT; // cast type
			match(token);
			while(token == Mul)
			{
				match(Mul);
				tmp = tmp + PTR;
			}
			match(')');
			expression(Inc); // cast has precedence as ++
			expr_type = tmp;
		}
		else
		{
			expression(Assign);
			match(')');
		}
	}
	else if(token == Mul) // dereference *<addr>
	{
		match(Mul);
		expression(Inc); // dereference has the same precedence as ++

		if(expr_type >= PTR)
			expr_type = expr_type - PTR;
		else
		{
			printf("%d: bad dereference\n", line);
			exit(-1);
		}

		*++text = (expr_type == CHAR) ? LC : LI;
	}
	else if(token == And) // get address
	{
		match(And);
		expression(Inc);

		if(*text == LC || *text == LI)
			--text;
		else
		{
			printf("%d: bad address of\n", line);
			exit(-1);
		}

		expr_type = expr_type + PTR;
	}
	else if(token == '!') // not
	{
		match('!');
		expression(Inc);

		// judge <expr> == 0
		*++text = PUSH;
		*++text = IMM;
		*++text = 0;
		*++text = EQ;

		expr_type = INT;
	}
	else if(token == '^') // bitwise not
	{
		match('~');
		expression(Inc);

		// use <expr> XOR -1
		*++text = PUSH;
		*++text = IMM;
		*++text = -1;
		*++text = XOR;

		expr_type = INT;
	}
	else if(token == Add) // +var, do nothing
	{
		match(Add);
		expression(Inc);
		expr_type = INT;
	}
	else if(token == Sub) // -var
	{
		match(Sub);

		if(token == Num)
		{
			*++text = IMM;
			*++text = -token_val;
			match(Num);
		}
		else
		{
			*++text = IMM;
			*++text = -1;
			*++text = PUSH;
			expression(Inc);
			*++text = MUL;
		}
		expr_type = INT;
	}
	else if(token == Inc || token == Dec) // ++ and --
	{
		tmp = token;
		match(token);
		expression(Inc);

		if(*text == LC)
		{
			*text = PUSH; // to duplicate the address
			*++text = LC;
		}
		else if(*text == LI)
		{
			*text = PUSH;
			*++text = LI;
		}
		else
		{
			printf("%d: bad lvalue of pre-increment\n", line);
			exit(-1);
		}
		*++text = PUSH;
		*++text = IMM;

		// pointer
		*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
		*++text = (tmp == Inc) ? ADD : SUB;
		*++text = (expr_type == CHAR) ? SC : SI;
	}
	else
	{
		printf("%d: bad expression\n", line);
		exit(-1);
	}

	while(token >= level) // parse token for binary operator and postfix operator
	{
		tmp = expr_type;
		if(token == Assign) // var = expr
		{
			match(Assign);
			if(*text == LC || *text == LI) // save the lvalue's pointer
				*text = PUSH;
			else
			{
				printf("%d: bad lvalue in assignment\n", line);
				exit(-1);
			}
			expression(Assign);

			expr_type = tmp;
			*++text = (expr_type == CHAR) ? SC : SI;
		}
		else if(token == Cond) // a ? b : c
		{
			match(Cond);
			*++text = JZ;
			addr = ++text;
			expression(Assign);
			if(token == ':')
				match(':');
			else
			{
				printf("%d: missing colon in conditional\n", line);
				exit(-1);
			}
			*addr = (int)(text + 3);
			*++text = JMP;
			addr = ++text;
			expression(Cond);
			*addr = (int)(text + 1);
		}
		else if(token == Lor) // logic or
		{
			match(Lor);
			*++text = JNZ;
			addr = ++text;
			expression(Lan);
			*addr = (int)(text + 1);
			expr_type = INT;
		}
		else if(token == Lan) // logic and
		{
			match(Lan);
			*++text = JZ;
			addr = ++text;
			expression(Or);
			*addr = (int)(text + 1);
			expr_type = INT;
		}
		else if(token == Or)
		{
			match(Or);
			*++text = PUSH;
			expression(Xor);
			*++text = OR;
			expr_type = INT;
		}
		else if(token == Xor)
		{
			match(Xor);
			*++text = PUSH;
			expression(And);
			*++text = XOR;
			expr_type = INT;
		}
		else if(token == And)
		{
			match(And);
			*++text = PUSH;
			expression(Eq);
			*++text = AND;
			expr_type = INT;
		}
		else if(token == Eq)
		{
			match(Eq);
			*++text = PUSH;
			expression(Lt);
			*++text = EQ;
			expr_type = INT;
		}
		else if(token == Ne)
		{
			match(Ne);
			*++text = PUSH;
			expression(Lt);
			*++text = NE;
			expr_type = INT;
		}
		else if(token == Lt)
		{
			match(Lt);
			*++text = PUSH;
			expression(Shl);
			*++text = LT;
			expr_type = INT;
		}
		else if(token == Gt)
		{
			match(Gt);
			*++text = PUSH;
			expression(Shl);
			*++text = GT;
			expr_type = INT;
		}
		else if(token == Le)
		{
			match(Le);
			*++text = PUSH;
			expression(Shl);
			*++text = LE;
			expr_type = INT;
		}
		else if(token == Ge)
		{
			match(Ge);
			*++text = PUSH;
			expression(Shl);
			*++text = GE;
			expr_type = INT;
		}
		else if(token == Shl)
		{
			match(Shl);
			*++text = PUSH;
			expression(Add);
			*++text = SHL;
			expr_type = INT;
		}
		else if(token == Shr)
		{
			match(Shr);
			*++text = PUSH;
			expression(Add);
			*++text = SHR;
			expr_type = INT;
		}
		else if(token == Add)
		{
			match(Add);
			*++text = PUSH;
			expression(Mul);

			expr_type = tmp;
			if(expr_type > PTR) // pointer type, and not 'char*'
			{
				*++text = PUSH;
				*++text = IMM;
				*++text = sizeof(int);
				*++text = MUL;
			}
			*++text = ADD;
		}
		else if(token == Sub)
		{
			match(Sub);
			*++text = PUSH;
			expression(Mul);

			if(tmp > PTR && expr_type == tmp)
			{
				*++text = SUB;
				*++text = PUSH;
				*++text = IMM;
				*++text = sizeof(int);
				*++text = DIV;
				expr_type = INT;
			}
			else if(tmp > PTR)
			{
				*++text = PUSH;
				*++text = IMM;
				*++text = sizeof(int);
				*++text = MUL;
				*++text = SUB;
				expr_type = tmp;
			}
			else
			{
				*++text = SUB;
				expr_type = tmp;
			}
		}
		else if(token == Mul)
		{
			match(Mul);
			*++text = PUSH;
			expression(Inc);
			*++text = MUL;
			expr_type = tmp;
		}
		else if(token == Div)
		{
			match(Div);
			*++text = PUSH;
			expression(Inc);
			*++text = DIV;
			expr_type = tmp;
		}
		else if(token == Mod)
		{
			match(Mod);
			*++text = PUSH;
			expression(Inc);
			*++text = MOD;
			expr_type = tmp;
		}
		else if(token == Inc || token == Dec)
		{
			if(*text == LC)
			{
				*text = PUSH; // to duplicate the address
				*++text = LC;
			}
			else if(*text == LI)
			{
				*text = PUSH;
				*++text = LI;
			}
			else
			{
				printf("%d: bad lvalue of pre-increment\n", line);
				exit(-1);
			}
			*++text = PUSH;
			*++text = IMM;

			*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
			*++text = (token == Inc) ? ADD : SUB;
			*++text = (expr_type == CHAR) ? SC : SI;

			*++text = PUSH;
			*++text = IMM;
			*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
			*++text = (token == Inc) ? SUB : ADD;
			match(token);
		}
		else if(token == Brak) // array access var[idx]
		{
			match(Brak);
			*++text = PUSH;
			expression(Assign);
			match(']');

			if(tmp > PTR) // pointer, not 'char*'
			{
				*++text = PUSH;
				*++text = IMM;
				*++text = sizeof(int);
				*++text = MUL;
			}
			else if(tmp < PTR)
			{
				printf("%d: pointer type expected\n", line);
				exit(-1);
			}
			expr_type = tmp - PTR;
			*++text = ADD;
			*++text = (expr_type == CHAR) ? LC : LI;
		}
		else
		{
			printf("%d: compiler error token = %d\n", line, token);
			exit(-1);
		}
	}
}

// analyse the statements in function body
void statement()
{
	int *a, *b;
	if(token == If)
	{
		match(If);
		match('(');
		expression(Assign); // parse condition
		match(')');

		*++text = JZ;
		b = ++text;

		statement();
		if(token == Else)
		{
			match(Else);

			// emit code for JMP B
			*b = (int)(text + 3);
			*++text = JMP;
			b = ++text;

			statement();
		}
		*b = (int)(text + 1);
	}
	else if(token == While)
	{
		match(While);
		a = text + 1;
		match('(');
		expression(Assign);
		match(')');

		*++text = JZ;
		b = ++text;

		statement();

		*++text = JMP;
		*++text = (int)a;
		*b = (int)(text + 1);
	}
	else if(token == Return)
	{
		match(Return);

		if(token != ';')
			expression(Assign);
		match(';');

		*++text = LEV;
	}
	else if(token == '{')
	{
		match('{');
		while(token != '}')
			statement();
		match('}');
	}
	else if(token == ';') // empty statement
	{
		match(';');
	}
	else // a=b; or function calling
	{
		expression(Assign);
		match(';');
	}
}

// analyse the function body
void function_body()
{
	// ...{
	// 1. local declarations
	// 2. statements
	// }

	int pos_local; // position of local variables on the stack
	int type;
	pos_local = index_of_bp;

	// local variables declaration, just like global ones
	while(token == Int || token == Char)
	{
		basetype = (token == Int) ? INT : CHAR;
		match(token);

		while(token != ';')
		{
			type = basetype;
			while(token == Mul)
			{
				match(Mul);
				type = type + PTR;
			}

			if(token != Id)
			{
				printf("%d: bad local declaration\n", line);
				exit(-1);
			}
			if(currnet_id[Class] == Loc) // identifier exists
			{
				printf("%d: duplicate local declaration\n", line);
				exit(-1);
			}
			match(Id);

			// store the local variable
			currnet_id[Bclass] = currnet_id[Class];
			currnet_id[Class] = Loc;
			currnet_id[Btype] = currnet_id[Type];
			currnet_id[Type] = type;
			currnet_id[Bvalue] = currnet_id[Value];
			currnet_id[Value] = ++pos_local;

			if(token == ',')
				match(',');
		}
		match(';');
	}

	// save the stack size for local variables
	*++text = ENT;
	*++text = pos_local - index_of_bp;

	// statements
	while(token != '}')
		statement();

	// emit code for leaving the sub function
	*++text = LEV;
}

// declaration of function
void function_declaration()
{
	// func_name(...){...}

	match('(');
	function_parameter();
	match(')');
	match('{');
	function_body();

	// unwind local variable declarations for all local variables
	currnet_id = symbols;
	while(currnet_id[Token])
	{
		if(currnet_id[Class] == Loc)
		{
			currnet_id[Class] = currnet_id[Bclass];
			currnet_id[Type] = currnet_id[Btype];
			currnet_id[Value] = currnet_id[Bvalue];
		}
		currnet_id = currnet_id + IdSize;
	}
}

// declaration of global variables
void global_declaration()
{
	// global declaration ::= enum_decl | variable_decl | function_decl
	//
	// enum_decl ::= 'enum' [id] '{' id ['=''num'], {','id['=''num'}'}'
	//
	// variable_decl ::= type {'*'} id {','{'*'}id}';'
	//
	// function_decl ::= type {'*'} id '('parameter_decl')''{'body_decl'}'

	int type; // tmp, actual type of variable
	int i; // tmp

	basetype = INT;

	// parse enum, this should be treated alone
	if(token == Enum) // enum[id]{a=10,b=20,c,d,...}
	{
		match(Enum);
		if(token != '{') // skip the id part
			match(Id);
		if(token == '{') // parse the assign part
		{
			match('{');
			enum_declaration();
			match('}');
		}
		match(';');
		return;
	}

	// parse type information
	if(token == Int)
	{
		match(Int);
	}
	else if(token == Char)
	{
		match(Char);
		basetype = CHAR;
	}

	// parse the comma seperated variable declaration
	while(token != ';' && token != '}')
	{
		type = basetype;
		// parse pointer type, note that there may exist 'int ****x;'
		while(token == Mul)
		{
			match(Mul);
			type = type + PTR;
		}

		if(token != Id) // invalid declaration
		{
			printf("%d: bad global declaration\n", line);
			exit(-1);
		}
		if(currnet_id[Class]) // identifier exists
		{
			printf("%d: duplicate global declaration\n", line);
			exit(-1);
		}
		match(Id);
		currnet_id[Type] = type;

		if(token == '(') // function declaration
		{
			currnet_id[Class] = Fun;
			currnet_id[Value] = (int)(text + 1); // the memory address of function
			function_declaration();
		}
		else // variable declaration
		{
			currnet_id[Class] = Glo; // global variable
			currnet_id[Value] = (int)data; // assign memory address
			data = data + sizeof(int);
		}

		if(token == ',')
			match(',');
	}
	next();
}

// the entrance of grammer analysis, analyse the whole C program
void program()
{
	next();
	while(token > 0)
	{
		global_declaration();
	}
}

// the entrance of virtual machine, to explain the object code
int eval()
{
	int op, *tmp;
	while(1)
	{
		op = *pc++;
		if(op == IMM) // load an immediate value to ax
			ax = *pc++;
		else if(op == LC) // load a character to ax, whose address is in ax
			ax = *(char*)ax;
		else if(op == LI) // load an integer to ax, whose address is in ax
			ax = *(int*)ax;
		else if(op == SC) // save a character to an address on the top of the stack, whose value is in ax
			ax = *(char*)*sp++ = ax;
		else if(op == SI) // save a integer to an address on the top of the stack, whose value is in ax
			*(int*)*sp++ = ax;
		else if(op == PUSH) // push the value of ax onto the stack
			*--sp = ax;
		else if(op == JMP) // jump to the objective address unconditionally
			pc = (int*)*pc;
		else if(op == JZ) // jump if ax is 0
			pc = ax ? pc + 1 : (int*)*pc;
		else if(op == JNZ) // jump if ax is not 0
			pc = ax ? (int*)*pc : pc + 1;
		else if(op == CALL) // call a subroutine
		{
			*--sp = (int)(pc + 1); // store the return address in the stack
			pc = (int*)*pc; // jump to the address of the subroutine
		}
		else if(op == ENT) // make the new stack frame for calling the subroutine
		{
			*--sp = (int)bp; // equal to push %ebp
			bp = sp; // equal to mov %esp, %ebp
			sp = sp - *pc++; // save stack for local variable
		}
		else if(op == ADJ) // equal to add <size>, %esp
			sp = sp + *pc++;
		else if(op == LEV) // restore call frame and pc
		{
			sp = bp; // equal to mov %ebp, %esp
			bp = (int*)*sp++; // equal to pop %ebp
			pc = (int*)*sp++; // get the return address, RET
		}
		else if(op == LEA) // load address for arguement
			ax = (int)(bp + *pc++);
		else if(op == OR)
			ax = *sp++ | ax;
		else if(op == XOR)
			ax = *sp++ ^ ax;
		else if(op == AND)
			ax = *sp++ & ax;
		else if(op == EQ)
			ax = *sp++ == ax;
		else if(op == NE)
			ax = *sp++ != ax;
		else if(op == LT)
			ax = *sp++ < ax;
		else if(op == GT)
			ax = *sp++ > ax;
		else if(op == LE)
			ax = *sp++ <= ax;
		else if(op == GE)
			ax = *sp++ >= ax;
		else if(op == SHR)
			ax = *sp++ >> ax;
		else if(op == SHL)
			ax = *sp++ << ax;
		else if(op == ADD)
			ax = *sp++ + ax;
		else if(op == SUB)
			ax = *sp++ - ax;
		else if(op == MUL)
			ax = *sp++ * ax;
		else if(op == DIV)
			ax = *sp++ / ax;
		else if(op == MOD)
			ax = *sp++ % ax;
		else if(op == EXIT) // exit()
		{
			printf("exit(%d)\n", *sp);
			return *sp;
		}
		else if(op == OPEN) // open()
			ax = open((char*)sp[1], sp[0]);
		else if(op == CLOS) // close()
			ax = close(*sp);
		else if(op == READ) // read()
			ax = read(sp[2], (char*)sp[1], *sp);
		else if(op == PRTF) // printf()
		{
			tmp = sp + pc[1];
			ax = printf((char*)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
		}
		else if(op == MALC) // malloc()
			ax = (int)malloc(*sp);
		else if(op == MSET) // memset()
			ax = (int)memset((char*)sp[2], sp[1], *sp);
		else if(op == MCMP) // memcmp()
			ax = memcmp((char*)sp[2],(char*)sp[1],*sp);
		else
		{
			printf("Unknown instruction: %d\n", op);
			exit(-1);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int i, fd;
	int *tmp;


	argc --;
	argv ++;

	poolSize = 256 * 1024; // arbitrary size
	line = 1;

	if((fd=open(*argv, 0)) < 0) // open source code failed
	{
		printf("Could not open the file %s\n", *argv);
		exit(-1);
	}

	// allocate memory for virtual machine
	if(!(text = old_text = malloc(poolSize))) // malloc memory for text segment failed
	{
		printf("Could not malloc %d for text segment\n", poolSize);
		exit(-1);
	}
	memset(text, 0, sizeof(text));

	if(!(data = malloc(poolSize))) // malloc memory for data segment failed
	{
		printf("Could not malloc %d for data segment\n", poolSize);
		exit(-1);
	}
	memset(data, 0, sizeof(data));

	if(!(stack = malloc(poolSize))) // malloc memory for stack failed
	{
		printf("Could not malloc %d for stack\n", poolSize);
		exit(-1);
	}
	memset(stack, 0, sizeof(stack));
	if(!(symbols = malloc(poolSize)))
	{
		printf("Could not malloc %d for symbol table\n", poolSize);
		exit(-1);
	}
	memset(symbols, 0, sizeof(symbols));

	// initialize the register
	bp = sp = (int *)((int)stack + poolSize); // point to the bottom of the stack
	ax = 0;

	// initialize the variable and function
	src = "char else enum if int return sizeof while open read close printf malloc memset memcmp exit void main";
	// add the keywords to symbol table
	i=Char;
	while(i <= While)
	{
		next();
		currnet_id[Token] = i++;
	}

	// add library to symbol table
	i = OPEN;
	while(i <= EXIT)
	{
		next();
		currnet_id[Class] = Sys;
		currnet_id[Type] = INT;
		currnet_id[Value] = i++;
	}
	next();
	currnet_id[Token] = Char; // void type
	next();
	idmain = currnet_id; // keep track of main

	// read the source file
    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    if (!(src = old_src = malloc(poolSize))) {
        printf("could not malloc(%d) for source area\n", poolSize);
        return -1;
    }
    // read the source file
    if ((i = read(fd, src, poolSize-1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0; // add EOF character
    close(fd);

	program();

	if(!(pc = (int*)idmain[Value]))
	{
		printf("main() not defined\n");
		exit(-1);
	}

	sp = (int*)((int)stack + poolSize);
	*--sp = EXIT; // call exit() when main() return
	*--sp = PUSH;
	tmp = sp;
	*--sp = argc;
	*--sp = (int)argv;
	*--sp = (int)tmp;

	return eval();
}
