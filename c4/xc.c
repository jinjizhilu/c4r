#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

int debug;    // print the executed instructions
int assembly; // print out the assembly and source

int token; // current token

// instructions
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

// tokens and classes (operators last and in precedence order)
// copied from c4
enum {
  C_Num = 128, C_Fun, C_Sys, C_Glo, C_Loc,
  Id, Num, 
  Char, Else, Enum, If, Int, Return, Sizeof, Struct, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak, Point
};

// fields of identifier
enum {Token, Hash, Name, FuncAddr, StructId, Type, Class, Value, Count, BStructId, BType, BClass, BValue, BCount, IdSize};

enum {S_Id, S_Count, S_Size, S_HeadSize};
enum {S_MemId, S_MemType, S_MemAddr, S_MemStructId, S_MemCount, S_MemSize};

// types of variable/function
enum { CHAR, INT, STRUCT, PTR };

// type of declaration.
enum {Global, Local};

int *text, // text segment
    *stack;// stack
int * old_text; // for dump text segment
char *data; // data segment
int *idmain;

char *src, *old_src;  // pointer to source code string;

int poolsize; // default size of text/data/stack
int *pc, *bp, *sp, ax, cycle; // virtual machine registers

int *current_id, // current parsed ID
    *symbols,    // symbol table
	*current_struct, // current parsed struct
	*structs,	 // struct table
    line,        // line number of source code
    token_val;   // value of current token (mainly for number)

int basetype;    // the type of a declaration, make it global for convenience
int expr_type;   // the type of an expression
int *last_struct;// to keep value between recursive expression processing

// function frame
//
// 0: arg 1
// 1: arg 2
// 2: arg 3
// 3: return address
// 4: old bp pointer  <- index_of_bp
// 5: local var 1
// 6: local var 2
int index_of_bp; // index of bp pointer on stack

void next() {
    char *last_pos;
    int hash;

    while (token = *src) {
        ++src;

        if (token == '\n') {
            if (assembly) {
                // print compile info
                printf("%d: %.*s", line, src-old_src, old_src);
                old_src = src;

                while (old_text < text) {
                    printf("%8.4s", & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                                      "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                                      "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[*++old_text * 5]);

                    if (*old_text <= ADJ)
                        printf(" %d\n", *++old_text);
                    else
                        printf("\n");
                }
            }
            ++line;
        }
        else if (token == '#') {
            // skip macro, because we will not support it
            while (*src != 0 && *src != '\n') {
                src++;
            }
        }
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')) {

            // parse identifier
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 147 + *src;
                src++;
            }

            // look for existing identifier, linear search
            current_id = symbols;
            while (current_id[Token]) {
                if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos)) {
                    //found one, return
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }


            // store new ID
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        }
        else if (token >= '0' && token <= '9') {
            // parse number, three kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if (token_val > 0) {
                // dec, starts with [1-9]
                while (*src >= '0' && *src <= '9') {
                    token_val = token_val*10 + *src++ - '0';
                }
            } else {
                // starts with number 0
                if (*src == 'x' || *src == 'X') {
                    //hex
                    token = *++src;
                    while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')) {
                        token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                } else {
                    // oct
                    while (*src >= '0' && *src <= '7') {
                        token_val = token_val*8 + *src++ - '0';
                    }
                }
            }

            token = Num;
            return;
        }
        else if (token == '/') {
            if (*src == '/') {
                // skip comments
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                // divide operator
                token = Div;
                return;
            }
        }
        else if (token == '"' || token == '\'') {
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data.
            last_pos = data;
            while (*src != 0 && *src != token) {
                token_val = *src++;
                if (token_val == '\\') {
                    // escape character
                    token_val = *src++;
                    if (token_val == 'n') {
                        token_val = '\n';
                    }
                }

                if (token == '"') {
                    *data++ = token_val;
                }
            }

            src++;
            // if it is a single character, return Num token
            if (token == '"') {
                token_val = (int)last_pos;
            } else {
                token = Num;
            }

            return;
        }
        else if (token == '=') {
            // parse '==' and '='
            if (*src == '=') {
                src ++;
                token = Eq;
            } else {
                token = Assign;
            }
            return;
        }
        else if (token == '+') {
            // parse '+' and '++'
            if (*src == '+') {
                src ++;
                token = Inc;
            } else {
                token = Add;
            }
            return;
        }
        else if (token == '-') {
            // parse '-' and '--'
            if (*src == '-') {
                src ++;
                token = Dec;
            } else {
                token = Sub;
            }
            return;
        }
        else if (token == '!') {
            // parse '!='
            if (*src == '=') {
                src++;
                token = Ne;
            }
            return;
        }
        else if (token == '<') {
            // parse '<=', '<<' or '<'
            if (*src == '=') {
                src ++;
                token = Le;
            } else if (*src == '<') {
                src ++;
                token = Shl;
            } else {
                token = Lt;
            }
            return;
        }
        else if (token == '>') {
            // parse '>=', '>>' or '>'
            if (*src == '=') {
                src ++;
                token = Ge;
            } else if (*src == '>') {
                src ++;
                token = Shr;
            } else {
                token = Gt;
            }
            return;
        }
        else if (token == '|') {
            // parse '|' or '||'
            if (*src == '|') {
                src ++;
                token = Lor;
            } else {
                token = Or;
            }
            return;
        }
        else if (token == '&') {
            // parse '&' and '&&'
            if (*src == '&') {
                src ++;
                token = Lan;
            } else {
                token = And;
            }
            return;
        }
        else if (token == '^') {
            token = Xor;
            return;
        }
        else if (token == '%') {
            token = Mod;
            return;
        }
        else if (token == '*') {
            token = Mul;
            return;
        }
        else if (token == '[') {
            token = Brak;
            return;
        }
        else if (token == '?') {
            token = Cond;
            return;
        }
		else if (token == '.') {
			token = Point;
			return;
		}
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') {
            // directly return the character as token;
            return;
        }
    }
}

void match(int tk) {
    if (token == tk) {
        next();
    } else {
        printf("%d: expected token: %d\n", line, tk);
        exit(-1);
    }
}

int find_struct(int *id) {
	int *struct_item;
	struct_item = structs;

	while (struct_item[S_Id] != 0) {
		if (struct_item[S_Id] == (int)id) {
			return (int)struct_item;
		}
		struct_item = struct_item + S_HeadSize + struct_item[S_Count] * S_MemSize;
	}
	return 0;
}

int find_struct_member(int *struct_item, int *id) {
	int *struct_end;
	struct_end = struct_item + S_HeadSize + struct_item[S_Count] * S_MemSize;
	struct_item = struct_item + S_HeadSize;

	while (struct_item < struct_end) {
		if (struct_item[S_MemId] == (int)id) {
			return (int)struct_item;
		}
		struct_item = struct_item + S_MemSize;
	}
	return 0;
}

int get_size(int type, int data) {
	int *addr;

	if (type >= PTR) {
		return sizeof(int);
	}
	else if (type == CHAR) {
		return sizeof(char);
	}
	else if (type == INT) {
		return sizeof(int);
	}
	else if (type == STRUCT) {
		addr = (int*)data;
		return addr[S_Size];
	}
	return 0;
} 

int align_to_int(int address) {
	address = (address + sizeof(int) - 1) / sizeof(int);
	return address;
}

int assign_type_check(int left, int right) {
	if (left >= PTR && right == INT) {
		return 1;
	}
	if ((left >= PTR && right >= PTR) && left != right) {
		return 0;
	}
	if (left == STRUCT || right == STRUCT) {
		return 0;
	}
	return 1;
}

void expression(int level) {
    // expressions have various format.
    // but majorly can be divided into two parts: unit and operator
    // for example `(char) *a[10] = (int *) func(b > 0 ? 10 : 20);
    // `a[10]` is an unit while `*` is an operator.
    // `func(...)` in total is an unit.
    // so we should first parse those unit and unary operators
    // and then the binary ones
    //
    // also the expression can be in the following types:
    //
    // 1. unit_unary ::= unit | unit unary_op | unary_op unit
    // 2. expr ::= unit_unary (bin_op unit_unary ...)

    // unit_unary()
    int *id, tmp, *addr, *struct_item;
	id = 0;

    {
        if (!token) {
            printf("%d: unexpected token EOF of expression\n", line);
            exit(-1);
        }
        if (token == Num) {
            match(Num);

            // emit code
            *++text = IMM;
            *++text = token_val;
            expr_type = INT;
        }
        else if (token == '"') {
            // continous string "abc" "abc"


            // emit code
            *++text = IMM;
            *++text = token_val;

            match('"');
            // store the rest strings
            while (token == '"') {
                match('"');
            }

            // append the end of string character '\0', all the data are default
            // to 0, so just move data one position forward.
			// align to 4 byte
            data = (char *)(align_to_int((int)data + sizeof(char)) * sizeof(int));
            expr_type = PTR;
        }
        else if (token == Sizeof) {
            // sizeof is actually an unary operator
            // now only `sizeof(int)`, `sizeof(char)` and `sizeof(*...)` are
            // supported.
            match(Sizeof);
            match('(');
            expr_type = INT;

            if (token == Int) {
                match(Int);
            } else if (token == Char) {
                match(Char);
                expr_type = CHAR;
            }

            while (token == Mul) {
                match(Mul);
                expr_type = expr_type + PTR;
            }

            match(')');

            // emit code
            *++text = IMM;
            *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

            expr_type = INT;
        }
        else if (token == Id) {
            // there are several type when occurs to Id
            // but this is unit, so it can only be
            // 1. function call
            // 2. Enum variable
            // 3. global/local variable

			last_struct = 0;
			id = current_id;
            match(Id);

            if (token == '(') {
                // function call
                match('(');

                // pass in arguments
                tmp = 0; // number of arguments
                while (token != ')') {
                    expression(Assign);
                    *++text = PUSH;
                    tmp ++;

                    if (token == ',') {
                        match(',');
                    }

                }
                match(')');

                // emit code
                if (id[Class] == C_Sys) {
                    // system functions
                    *++text = id[Value];
                }
                else if (id[Class] == C_Fun) {
                    // function call
                    *++text = CALL;
					*++text = id[Value];
                }
                else {
                    printf("%d: bad function call\n", line);
                    exit(-1);
                }

                // clean the stack for arguments
                if (tmp > 0) {
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id[Type];
            }
            else if (id[Class] == C_Num) {
                // enum variable
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            }
            else {
                // variable
                if (id[Class] == C_Loc) {
                    *++text = LEA;
                    *++text = index_of_bp - id[Value];
                }
                else if (id[Class] == C_Glo) {
                    *++text = IMM;
                    *++text = id[Value];
                }
                else {
                    printf("%d: undefined variable\n", line);
                    exit(-1);
                }

				last_struct = (int*)find_struct((int*)id[StructId]);

				if (id[Count] == 0)
				{
					// emit code, default behaviour is to load the value of the
					// address which is stored in `ax`
					expr_type = id[Type];
					*++text = (expr_type == Char) ? LC : LI;
				}
				else {
					// for array type, value is address, do not need LC/LI
					expr_type = id[Type] + PTR;
				}
            }
        }
        else if (token == '(') {
            // cast or parenthesis
            match('(');
            if (token == Int || token == Char) {
                tmp = (token == Char) ? CHAR : INT; // cast type
                match(token);
                while (token == Mul) {
                    match(Mul);
                    tmp = tmp + PTR;
                }

                match(')');

                expression(Inc); // cast has precedence as Inc(++)

                expr_type  = tmp;
            } else {
                // normal parenthesis
                expression(Assign);
                match(')');
            }
        }
        else if (token == Mul) {
            // dereference *<addr>
            match(Mul);
            expression(Inc); // dereference has the same precedence as Inc(++)

            if (expr_type >= PTR) {
                expr_type = expr_type - PTR;
            } else {
                printf("%d: bad dereference\n", line);
                exit(-1);
            }

            *++text = (expr_type == CHAR) ? LC : LI;
        }
        else if (token == And) {
            // get the address of
            match(And);
            expression(Inc); // get the address of
            if (*text == LC || *text == LI) {
                text --;
            } else {
                printf("%d: bad address of\n", line);
                exit(-1);
            }

            expr_type = expr_type + PTR;
        }
        else if (token == '!') {
            // not
            match('!');
            expression(Inc);

            // emit code, use <expr> == 0
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            expr_type = INT;
        }
        else if (token == '~') {
            // bitwise not
            match('~');
            expression(Inc);

            // emit code, use <expr> XOR -1
            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            expr_type = INT;
        }
        else if (token == Add) {
            // +var, do nothing
            match(Add);
            expression(Inc);

            expr_type = INT;
        }
        else if (token == Sub) {
            // -var
            match(Sub);

            if (token == Num) {
                *++text = IMM;
                *++text = -token_val;
                match(Num);
            } else {

                *++text = IMM;
                *++text = -1;
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
            }

            expr_type = INT;
        }
        else if (token == Inc || token == Dec) {
            tmp = token;
            match(token);
            expression(Inc);
            if (*text == LC) {
                *text = PUSH;  // to duplicate the address
                *++text = LC;
            } else if (*text == LI) {
                *text = PUSH;
                *++text = LI;
            } else {
                printf("%d: bad lvalue of pre-increment\n", line);
                exit(-1);
            }
            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp == Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;
        }
        else {
            printf("%d: bad expression\n", line);
            exit(-1);
        }
    }

    // binary operator and postfix operators.
    {
        while (token >= level) {
            // handle according to current operator's precedence
            tmp = expr_type;
            if (token == Assign) {
                // var = expr;
                match(Assign);
                if (*text == LC || *text == LI) {
                    *text = PUSH; // save the lvalue's pointer
                } else {
                    printf("%d: bad lvalue in assignment\n", line);
                    exit(-1);
                }
                expression(Assign);

				if (assign_type_check(tmp, expr_type) == 0) {
					printf("%d: unmatched type in assign\n", line);
					exit(-1);
				}

                expr_type = tmp;
                *++text = (expr_type == CHAR) ? SC : SI;
            }
            else if (token == Cond) {
                // expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);
                if (token == ':') {
                    match(':');
                } else {
                    printf("%d: missing colon in conditional\n", line);
                    exit(-1);
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Cond);
                *addr = (int)(text + 1);
            }
            else if (token == Lor) {
                // logic or
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int)(text + 1);
                expr_type = INT;
            }
            else if (token == Lan) {
                // logic and
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int)(text + 1);
                expr_type = INT;
            }
            else if (token == Or) {
                // bitwise or
                match(Or);
                *++text = PUSH;
                expression(Xor);
                *++text = OR;
                expr_type = INT;
            }
            else if (token == Xor) {
                // bitwise xor
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                expr_type = INT;
            }
            else if (token == And) {
                // bitwise and
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                expr_type = INT;
            }
            else if (token == Eq) {
                // equal ==
                match(Eq);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                expr_type = INT;
            }
            else if (token == Ne) {
                // not equal !=
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = NE;
                expr_type = INT;
            }
            else if (token == Lt) {
                // less than
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                expr_type = INT;
            }
            else if (token == Gt) {
                // greater than
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT;
                expr_type = INT;
            }
            else if (token == Le) {
                // less than or equal to
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                expr_type = INT;
            }
            else if (token == Ge) {
                // greater than or equal to
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                expr_type = INT;
            }
            else if (token == Shl) {
                // shift left
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                expr_type = INT;
            }
            else if (token == Shr) {
                // shift right
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                expr_type = INT;
            }
            else if (token == Add) {
                // add
                match(Add);
                *++text = PUSH;
                expression(Mul);

                expr_type = tmp;
                if (expr_type > PTR) {
                    // pointer type, and not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            }
            else if (token == Sub) {
                // sub
                match(Sub);
                *++text = PUSH;
                expression(Mul);
                if (tmp > PTR && tmp == expr_type) {
                    // pointer subtraction
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                } else if (tmp > PTR) {
                    // pointer movement
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                } else {
                    // numeral subtraction
                    *++text = SUB;
                    expr_type = tmp;
                }
            }
            else if (token == Mul) {
                // multiply
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            }
            else if (token == Div) {
                // divide
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = DIV;
                expr_type = tmp;
            }
            else if (token == Mod) {
                // Modulo
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            }
            else if (token == Inc || token == Dec) {
                // postfix inc(++) and dec(--)
                // we will increase the value to the variable and decrease it
                // on `ax` to get its original value.
                if (*text == LI) {
                    *text = PUSH;
                    *++text = LI;
                }
                else if (*text == LC) {
                    *text = PUSH;
                    *++text = LC;
                }
                else {
                    printf("%d: bad value in increment\n", line);
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
            else if (token == Brak) {
                // array access var[xx]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');
					
				if (tmp > PTR) {
					// pointer, `not char *`
					*++text = PUSH;
					*++text = IMM;
					*++text = sizeof(int);
					*++text = MUL;
				}
				else if (tmp < PTR) {
					printf("%d: pointer or array type expected\n", line);
					exit(-1);
				}
				expr_type = tmp - PTR;
				*++text = ADD;
				*++text = (expr_type == CHAR) ? LC : LI;
            }
			else if (token == Point) {
				// struct member access var1.var2

				if (tmp != STRUCT || last_struct == 0) {
					printf("%d: struct type expected\n", line);
					exit(-1);
				}
				struct_item = (int*)last_struct;
				match(Point);

				struct_item = (int*)find_struct_member(struct_item, current_id);
				match(Id);

				expr_type = struct_item[S_MemType];

				if (expr_type == STRUCT) {
					last_struct = (int*)find_struct((int*)struct_item[S_MemStructId]);
				}

				if (*text == LC || *text == LI) {
					*text = PUSH;
				} else {
					printf("%d: bad lvalue in assignment\n", line);
					exit(-1);
				}
				*++text = IMM;
				*++text = struct_item[S_MemAddr];
				*++text = ADD;

				if (struct_item[S_MemCount] == 0)
				{
					*++text = (expr_type == CHAR) ? LC : LI;
				}
				else {
					expr_type = expr_type + PTR;
				}
			}
            else {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }
    }
}

void statement() {
    // there are 8 kinds of statements here:
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a, *b; // bess for branch control

    if (token == If) {
        // if (...) <statement> [else <statement>]
        //
        //   if (...)           <cond>
        //                      JZ a
        //     <statement>      <statement>
        //   else:              JMP b
        // a:
        //     <statement>      <statement>
        // b:                   b:
        //
        //
        match(If);
        match('(');
        expression(Assign);  // parse condition
        match(')');

        // emit code for if
        *++text = JZ;
        b = ++text;

        statement();         // parse statement
        if (token == Else) { // parse else
            match(Else);

            // emit code for JMP B
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            statement();
        }

        *b = (int)(text + 1);
    }
    else if (token == While) {
        //
        // a:                     a:
        //    while (<cond>)        <cond>
        //                          JZ b
        //     <statement>          <statement>
        //                          JMP a
        // b:                     b:
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
    else if (token == '{') {
        // { <statement> ... }
        match('{');

        while (token != '}') {
            statement();
        }

        match('}');
    }
    else if (token == Return) {
        // return [expression];
        match(Return);

        if (token != ';') {
            expression(Assign);
        }

        match(';');

        // emit code for return
        *++text = LEV;
    }
    else if (token == ';') {
        // empty statement
        match(';');
    }
    else {
        // a = b; or function_call();
        expression(Assign);
        match(';');
    }
}

void enum_declaration() {
    // parse enum [id] { a = 1, b = 3, ...}
    int i;
    i = 0;
    while (token != '}') {
        if (token != Id) {
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        next();
        if (token == Assign) {
            // like {a=10}
            next();
            if (token != Num) {
                printf("%d: bad enum initializer\n", line);
                exit(-1);
            }
            i = token_val;
            next();
        }

        current_id[Class] = C_Num;
        current_id[Type] = INT;
        current_id[Value] = i++;
		current_id[Count] = 0;

        if (token == ',') {
            next();
        }
    }
}

void struct_declaration(int *id) {
	// parse struct id {int a, char b, int* c, ...}
	int *struct_head, *struct_item, *struct_id, count, type;

	count = 0;
	struct_id = 0;
	struct_item = 0;

	struct_head = current_struct;
	current_struct[S_Id] = (int)id;
	current_struct[S_Count] = 0;
	current_struct[S_Size] = 0;
	current_struct = current_struct + S_HeadSize;

	while (token != '}') {
		// parse type information
		basetype = INT;
		if (token == Char) basetype = CHAR;
		if (token == Struct) basetype = STRUCT;

		match(token);

		// parse the comma seperated variable declaration.
		while (token != ';') {
			type = basetype;
			// parse pointer type, note that there may exist `int ****x;`
			while (token == Mul) {
				match(Mul);
				type = type + PTR;
			}

			if (token != Id) {
				// invalid declaration
				printf("%d: bad struct member declaration\n", line);
				exit(-1);
			}

			if (basetype == STRUCT) {
				struct_id = current_id;
				struct_item = (int*)find_struct(current_id);
				match(Id);
			}

			// struct member declaration
			current_struct[S_MemId] = (int)current_id;
			current_struct[S_MemType] = type;
			current_struct[S_MemAddr] = struct_head[S_Size];
			current_struct[S_MemCount] = 0;
			current_struct[S_MemStructId] = (int)struct_id;
			count = 1;

			match(Id);

			if (token == Brak) {
				// array declaration
				match(Brak);
				if (token == Num && token_val > 0) {
					count = token_val;
					current_struct[S_MemCount] = token_val;
				}
				else {
					printf("%d: bad array declaration\n", line);
					exit(-1);
				}
				match(Num);
				match(']');
			}

			struct_head[S_Size] = struct_head[S_Size] + get_size(type, (int)struct_item) * count;
			struct_head[S_Count] = struct_head[S_Count] + 1;
			current_struct = current_struct + S_MemSize;

			if (token == ',') {
				match(',');
			}
		}
		match(';');
	}
	
}

void function_parameter() {
    int type;
    int params;
    params = 0;
    while (token != ')') {
        // int name, ...
        type = INT;
        if (token == Int) {
            match(Int);
        } else if (token == Char) {
            type = CHAR;
            match(Char);
        }

        // pointer type
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        // parameter name
        if (token != Id) {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (current_id[Class] == C_Loc) {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }

        // store the local variable
        current_id[BClass] = current_id[Class]; current_id[Class]  = C_Loc;
        current_id[BType]  = current_id[Type];  current_id[Type]   = type;
        current_id[BValue] = current_id[Value]; current_id[Value]  = params++;   // index of current parameter

		match(Id);

        if (token == ',') {
            match(',');
        }
    }
    index_of_bp = params+1;
}

void function_body() {
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    int pos_local, old_pos, type, *id, *struct_item;; // position of local variables on the stack.

	struct_item = 0;
    pos_local = index_of_bp;

    while (token == Int || token == Char || token == Struct) {
        // local variable declaration, just like global ones.
        basetype = INT;
		if (token == Char) basetype = CHAR;
		if (token == Struct) basetype = STRUCT;

        match(token);

        while (token != ';') {
            type = basetype;
            while (token == Mul) {
                match(Mul);
                type = type + PTR;
            }

            if (token != Id) {
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }

			if (basetype == STRUCT) {
				id = current_id;
				struct_item = (int*)find_struct(current_id);
				match(Id);
			}

            if (current_id[Class] == C_Loc) {
                // identifier exists
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }

			old_pos = pos_local;
            // store the local variable
            current_id[BClass] = current_id[Class]; current_id[Class]  = C_Loc;
            current_id[BType]  = current_id[Type];  current_id[Type]   = type;
            current_id[BValue] = current_id[Value]; current_id[Value]  = ++pos_local;   // index of current parameter
			current_id[BCount] = current_id[Count]; current_id[Count] = 0;
			current_id[BStructId] = current_id[StructId]; current_id[StructId] = 0;

			if (basetype == STRUCT) {
				current_id[StructId] = (int)id;
				pos_local = old_pos + align_to_int(struct_item[S_Size]);
				current_id[Value] = pos_local;
			}

			match(Id);

			if (token == Brak) {
				// array declaration
				match(Brak);
				if (token == Num && token_val > 0) {
					current_id[Count] = token_val;

					// allocate memory & align to 4 byte
					pos_local = old_pos + align_to_int(get_size(type, (int)struct_item) * (token_val));
					current_id[Value] = pos_local;
				}
				else {
					printf("%d: bad array declaration\n", line);
					exit(-1);
				}
				match(Num);
				match(']');
			}

            if (token == ',') {
                match(',');
            }
        }
        match(';');
    }

    // save the stack size for local variables
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    // statements
    while (token != '}') {
        statement();
    }

    // emit code for leaving the sub function
    *++text = LEV;
}

void function_declaration() {
    // type func_name (...)
	int *id;
	id = current_id;

    match('(');
    function_parameter();
    match(')');

	if (token == ';') {
		// declaration	
		*++text = JMP;
		*++text = 0;
		// store jmp target address
		id[FuncAddr] = (int)text;
		//match(';');
	}
	else {
		// define
		match('{');

		// set jmp target address
		if (id[FuncAddr] != 0) {
			*(int*)(id[FuncAddr]) = (int)(text + 1);
			id[FuncAddr] = 0;
		}

		function_body();
		//match('}');
	}
	// unwind local variable declarations for all local variables.
	current_id = symbols;
	while (current_id[Token]) {
		if (current_id[Class] == C_Loc) {
			current_id[Class] = current_id[BClass];
			current_id[Type]  = current_id[BType];
			current_id[Value] = current_id[BValue];
			current_id[Count] = current_id[BCount];
			current_id[StructId] = current_id[BStructId];
		}
		current_id = current_id + IdSize;
	}
}

void global_declaration() {
    // int [*]id [; | (...) {...}]
    int type, *id, *struct_item; // tmp, actual type for variable

	id = 0;
	struct_item = 0;
    basetype = INT;

    // parse enum, this should be treated alone.
    if (token == Enum) {
        // enum [id] { a = 10, b = 20, ... }
        match(Enum);
        if (token != '{') {
            match(Id); // skip the [id] part
        }
        if (token == '{') {
            // parse the assign part
            match('{');
            enum_declaration();
            match('}');
        }

        match(';');
        return;
    }

	// parse struct, this should be treated alone
	if (token == Struct) {
		match(Struct);
		
		id = current_id;
		struct_item = (int*)find_struct(id);
		basetype = STRUCT;

		match(Id);

		// struct declaration
		if (token == '{') {
			match('{');
			struct_declaration(id);
			match('}');
			match(';');
			return;
		}

		if (struct_item == 0) {
			printf("%d: undefined struct type\n", line);
			exit(-1);
		}
	}

    // parse type information
	if (basetype == STRUCT) {
		// struct variables would be handled below
	}
    else if (token == Int) {
        match(Int);
    }
    else if (token == Char) {
        match(Char);
        basetype = CHAR;
    }

    // parse the comma seperated variable declaration.
    while (token != ';' && token != '}') {
        type = basetype;
        // parse pointer type, note that there may exist `int ****x;`
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        if (token != Id) {
            // invalid declaration
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
		// current_id[FuncAddr] != 0 means function declaration has been processed
        if (current_id[Class] && current_id[FuncAddr] == 0) {
            // identifier exists
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }

		current_id[Type] = type;
		current_id[StructId] = (int)id;
		id = current_id;

		match(Id); 

        if (token == '(') {
            id[Class] = C_Fun;
            id[Value] = (int)(text + 1); // the memory address of function
			id[Count] = 0;
            function_declaration();
        } 
		else if (token == Brak) {
			// array declaration
			match(Brak);
			if (token == Num && token_val >	0) {
				id[Class] = C_Glo; // global variable
				id[Value] = (int)data; // assign memory address
				id[Count] = token_val;
				
				// allocate memory & align to 4 byte
				data = data + get_size(type, (int)struct_item) * (token_val);
				data = (char*)(align_to_int((int)data) * sizeof(int));
			}
			else {
				printf("%d: bad array declaration\n", line);
				exit(-1);
			}
			match(Num);
			match(']');
		}
		else if (token == ',' || token == ';') {
            // variable declaration
            id[Class] = C_Glo; // global variable
            id[Value] = (int)data; // assign memory address
			id[Count] = 0;
			data = data + get_size(type, (int)struct_item);
			data = (char*)(align_to_int((int)data) * sizeof(int));
        }

        if (token == ',') {
            match(',');
        }
    }
    next();
}

void program() {
    // get next token
    next();
    while (token > 0) {
        global_declaration();
    }
}

int eval() {
    int op, *tmp;
    cycle = 0;
    while (1) {
        cycle ++;
        op = *pc++; // get next operation code

        // print debug info
        if (debug) {
            printf("%d> %.4s", cycle,
                   & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                   "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                   "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[op * 5]);
            if (op <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
			printf("ax:%d stack:%d\n", ax, *sp);
        }
        if (op == IMM)       {ax = *pc++;}                                     // load immediate value to ax
        else if (op == LC)   {ax = *(char *)ax;}                               // load character to ax, address in ax
        else if (op == LI)   {ax = *(int *)ax;}                                // load integer to ax, address in ax
        else if (op == SC)   {*(char *)*sp++ = ax;}                       // save character to address, value in ax, address on stack
        else if (op == SI)   {*(int *)*sp++ = ax;}                             // save integer to address, value in ax, address on stack
        else if (op == PUSH) {*--sp = ax;}                                     // push the value of ax onto the stack
        else if (op == JMP)  {pc = (int *)*pc;}                                // jump to the address
        else if (op == JZ)   {pc = ax ? pc + 1 : (int *)*pc;}                   // jump if ax is zero
        else if (op == JNZ)  {pc = ax ? (int *)*pc : pc + 1;}                   // jump if ax is zero
        else if (op == CALL) {*--sp = (int)(pc+1); pc = (int *)*pc;}           // call subroutine
        //else if (op == RET)  {pc = (int *)*sp++;}                              // return from subroutine;
        else if (op == ENT)  {*--sp = (int)bp; bp = sp; sp = sp - *pc++;}      // make new stack frame
        else if (op == ADJ)  {sp = sp + *pc++;}                                // add esp, <size>
        else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;}  // restore call frame and PC
        else if (op == LEA)  {ax = (int)(bp + *pc++);}                         // load address for arguments.

        else if (op == OR)  ax = *sp++ | ax;
        else if (op == XOR) ax = *sp++ ^ ax;
        else if (op == AND) ax = *sp++ & ax;
        else if (op == EQ)  ax = *sp++ == ax;
        else if (op == NE)  ax = *sp++ != ax;
        else if (op == LT)  ax = *sp++ < ax;
        else if (op == LE)  ax = *sp++ <= ax;
        else if (op == GT)  ax = *sp++ >  ax;
        else if (op == GE)  ax = *sp++ >= ax;
        else if (op == SHL) ax = *sp++ << ax;
        else if (op == SHR) ax = *sp++ >> ax;
        else if (op == ADD) ax = *sp++ + ax;
        else if (op == SUB) ax = *sp++ - ax;
        else if (op == MUL) ax = *sp++ * ax;
        else if (op == DIV) ax = *sp++ / ax;
        else if (op == MOD) ax = *sp++ % ax;

        else if (op == EXIT) { printf("exit(%d)", *sp); return *sp;}
        else if (op == OPEN) { ax = open((char *)sp[1], sp[0]); }
        else if (op == CLOS) { ax = close(*sp);}
        else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
        else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }
        else if (op == MALC) { ax = (int)malloc(*sp);}
        else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp);}
        else if (op == MCMP) { ax = memcmp((char *)sp[2], (char *)sp[1], *sp);}
        else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
}

int main(int argc, char **argv)
{
    int i, fd;
    int *tmp;

    argc--;
    argv++;

    // parse arguments
    if (argc > 0 && **argv == '-' && (*argv)[1] == 's') {
        assembly = 1;
        --argc;
        ++argv;
    }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') {
        debug = 1;
        --argc;
        ++argv;
    }
    if (argc < 1) {
        printf("usage: xc [-s] [-d] file ...\n");
        return -1;
    }
	//assembly = 1;
	//debug = 1;

    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    poolsize = 256 * 1024; // arbitrary size
    line = 1;

    // allocate memory
    if (!(text = malloc(poolsize))) {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize))) {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize))) {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    if (!(symbols = malloc(poolsize))) {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }
	if (!(structs = malloc(poolsize))) {
		printf("could not malloc(%d) for struct table\n", poolsize);
		return -1;
	}

    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);
    memset(structs, 0, poolsize);

	current_struct = structs;

    old_text = text;

    src = "char else enum if int return sizeof struct while "
          "open read close printf malloc memset memcmp exit void main";

     // add keywords to symbol table
    i = Char;
    while (i <= While) {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= EXIT) {
        next();
        current_id[Class] = C_Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
		current_id[Count] = 0;
    }

    next(); current_id[Token] = Char; // handle void type
    next(); idmain = current_id; // keep track of main

    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    // read the source file
    if ((i = read(fd, src, poolsize-1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0; // add EOF character
    close(fd);

    program();

    if (!(pc = (int *)idmain[Value])) {
        printf("main() not defined\n");
        return -1;
    }

    // dump_text();
    if (assembly) {
        // only for compile
        return 0;
    }

    // setup stack
    sp = (int *)((int)stack + poolsize);
    *--sp = EXIT; // call exit if main returns
    *--sp = PUSH; tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;

    return eval();
}