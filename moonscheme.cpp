#include "moonscheme.hpp"

namespace moonscheme{

cell_t parser::read_expr_list(cell_t ret, char end, int indent){
	cell_t p = ret;
	cell_t tmp;

	for(;;){
		eat_delim_ = true;
		token_ = LEX();
		eat_delim_ = false;
		if(token_ == end)
			break;

		hold_token_ = true;
		tmp = read_expr2();
		if(tmp)
			p = ((ret ? reinterpret_cast<pair_cell*>(p)->cdr : ret) = MAKE_PAIR(tmp, null_cell));

		token_ = LEX();
		if(token_ == end)
			break;
		else{
			switch(token_){
			case EOB: throw "unfinished expr list";
			case BL:
			case NEWLINE: continue;
			default: throw "no expr list delim";
			}
		}
	}
	return ret ? ret : null_cell;
}

template<class T>
inline std::size_t hash_pointer(const T* p){
	std::size_t x = static_cast<std::size_t>(reinterpret_cast<uint_t>(p));
	return x + (x >> 3);
}
template<class It>
inline std::size_t hash_range(std::size_t seed, It first, It last){
	for(; first != last; ++first)
		seed ^= *first + 0x9e3779b9 + (seed<<6) + (seed>>2);
	return seed;
}

#define DEFINE_FIXED_SYMBOL(name, literal) \
	static cell_t symbol_##name##_;

DEFINE_FIXED_SYMBOL(quote, "quote")
DEFINE_FIXED_SYMBOL(quasiquote, "quasiquote")
DEFINE_FIXED_SYMBOL(unquote, "unquote")
DEFINE_FIXED_SYMBOL(unquote_splicing, "unquote-splicing")
DEFINE_FIXED_SYMBOL(dot3, "...")
DEFINE_FIXED_SYMBOL(dot, ".")
DEFINE_FIXED_SYMBOL(backslash, "\\")
DEFINE_FIXED_SYMBOL(empty, "")
DEFINE_FIXED_SYMBOL(self, "self")
DEFINE_FIXED_SYMBOL(vector, "vector")
DEFINE_FIXED_SYMBOL(table, "table")
DEFINE_FIXED_SYMBOL(__index, "__index")

#define ISYMBOL(name) symbol_##name##_

enum Terminal{
	// char
	OTHER,
	SEMICOLON,
	EOB,
	LR,
	LN,

	BL,
	BAR,
	DQUOTE,
	LP,
	RP,

	DOT,
	BACKSLASH,
	EX,
	LBR,
	RBR,

	AT,
	QUOTE,
	BACKQUOTE,
	UNQUOTE,
	SHARP,

	COLON,
	LCBR,
	RCBR,

	// terminal
	SYMBOL,
	NEWLINE,
	DOT3,
	SDOT,
	UNQUOTE_SPLICING,
	CONSTANT,
	VECTOR,
	EXPR_COMMENT,

	TERMINAL_SIZE
};

static const char char_map[256] = {
EOB,  0,  0,  0,  0,  0,  0,  0,  0, BL, LN,  0,  0, LR,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 BL, EX, DQUOTE, SHARP,  0,  0,  0, QUOTE, LP, RP,  0,  0, UNQUOTE,  0, DOT,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, COLON,  SEMICOLON,  0,  0,  0,  0,
 AT,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, LBR, BACKSLASH, RBR, 0,  0,
BACKQUOTE,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  LCBR,  BAR,  RCBR,  0,  0,

  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

#define READ() (c = char_map[(unsigned char)*begin_])

void parser::append_string_escape(){
	switch(*++begin_){
	// c
	case '\'': buf_.push_back('\x27'); break;
	case '\"': buf_.push_back('\x22'); break;
	case '\?': buf_.push_back('\x3f'); break;
	case '\\': buf_.push_back('\x5c'); break;
	case 'a': buf_.push_back('\x07'); break;
	case 'b': buf_.push_back('\x08'); break;
	case 'f': buf_.push_back('\x0c'); break;
	case 'n': buf_.push_back('\x0a'); break;
	case 'r': buf_.push_back('\x0d'); break;
	case 't': buf_.push_back('\x09'); break;
	case 'v': buf_.push_back('\x0b'); break;

	// case '0': buf_.push_back('\0'); break; // TODO: oct
	case 'x': buf_.push_back('\0'); break; // TODO: hex
	case 'u': buf_.push_back('\0'); break; // TODO: unicode 16
	case 'U': buf_.push_back('\0'); break; // TODO: unicode 32

	// ext
	case '|': buf_.push_back('|'); break;
	default: throw "illegal escape character";
	}
	++begin_;
}

bool parser::read_literal(char end){
	char c;
	value_ = 0;
	buf_.clear();

	for(;;){
		READ();
		// ++begin_;
		if(c == end)
			break;
		switch(c){
		case EOB:
		case LR:
		case LN: return false;
		case BACKSLASH:	append_string_escape(); continue;
		default: buf_.push_back(*begin_); ++begin_; continue;
		}
	}

	++begin_;
	if(!value_)
		value_ = make_symbol(buf_.data(), buf_.data()+buf_.size());
	return true;
}

char parser::lex(){
	DM("begin lex")
	decltype(begin_) b;
	int_t block_depth;
	char c;

	READ();
__lex_1__:
	DM("__lex_1__: "<<*begin_<<" : "<<(int)*begin_)
	if(begin_ > end_)
		throw "eof";
	// if(!*begin_)
	// 	throw "eof";
	++begin_;
	switch(c){
	case OTHER:
		b = begin_ - 1;
		while(READ() == OTHER) ++begin_;
		value_ = make_symbol(b, begin_);
		DM("lex symbol: "<< std::string(b, begin_))
		return SYMBOL;

	case SEMICOLON:
		++begin_;
		for(;;){
			READ();
			++begin_;
			switch(c){
			case EOB: return EOB;
			case LR: goto __lex_2__;
			case LN: goto __lex_3__;
			default: continue;
			}
		}

	// EOB,
	case LR:
__lex_2__:
		if(READ() == LN) ++begin_;
	case LN:
__lex_3__:
		++line_;
		if(eat_delim_){
			READ();
			goto __lex_1__;
		}
		else
			return NEWLINE;

	case BL:
		while(READ() == BL) ++begin_;
		if(eat_delim_)
			goto __lex_1__;
		else
			return BL;

	case BAR:
		if(!read_literal(BAR))
			throw "illegal |...| symbol";
		return SYMBOL;

	case DQUOTE:
		if(!read_literal(DQUOTE))
			throw "illegal string";
		return CONSTANT;
		// buf_.clear();
		// for(;;){
		// 	switch(READ()){
		// 	case EOB:
		// 	case LR:
		// 	case LN: throw "unfinished string";
		// 	case DQUOTE: ++begin_; value_ = make_symbol(buf_.begin(), buf_.end()); return CONSTANT;
		// 	case BACKSLASH:	append_string_escape(); continue;
		// 	default: buf_.push_back(*begin_); ++begin_; continue;
		// 	}
		// }

	// LP,
	// RP,

	case DOT:
		DM("lex DOT: "<<(int)*(begin_-1)<<' '<<(int)*begin_<<' '<<(int)*(begin_+1))
		switch(READ()){
		case EOB:
		case LR:
		case LN:
		case BL:
		case SEMICOLON:
		case RP:
		case RBR:
		case RCBR:
			return SDOT;
		case DOT: break;
		default: return DOT;
		}
		++begin_;
		if(READ() != DOT) throw ".. is not allow";
		++begin_;
		return DOT3; // TODO: like SDOT

	// BACKSLASH,
	// EX,
	// LBR,
	// RBR,

	// AT,
	// QUOTE,
	// BACKQUOTE,
	case UNQUOTE:
		if(READ() == AT){ ++begin_; return UNQUOTE_SPLICING; }
		else return UNQUOTE;

	case SHARP:
		// READ()
		c = *begin_;
		++begin_;
		switch(c){
		case ';': return EXPR_COMMENT;
		case '\\': // TODO: escape
		case '|': // block comment
			DM("block comment")
			block_depth = 1;
			for(;;){
				READ();
				++begin_;
				switch(c){
				case LR:
					if(READ() == LN) ++begin_;
				case LN:
					++line_;
					break;

				case BAR:
					if(READ() == SHARP){
						++begin_;
						if(--block_depth <= 0){
							if(eat_delim_){
								READ();
								goto __lex_1__;
							}
							else
								return BL; // notice here
						}
					}
					break;
				case SHARP:
					if(READ() == BAR){
						++begin_;
						++block_depth;
					}
					break;
				}
			}

		case '(': return VECTOR;
		case 't': value_ = true_cell; return CONSTANT;
		case 'f': value_ = false_cell; return CONSTANT;
		}

	default: return c;
	}
}

#define MAKE_PAIR(a, b) reinterpret_cast<cell_t>(new pair_cell{pair_type, (a), (b)})
#define MAKE_LIST2(a, b) MAKE_PAIR(a, MAKE_PAIR(b, null_cell))
#define MAKE_LIST3(a, b, c) MAKE_PAIR(a, MAKE_PAIR(b, MAKE_PAIR(c, null_cell)))

#define LEX() (hold_token_ ? hold_token_ = false, token_ : token_ = lex())

#define SWITCH_LEX(n) \
__##n##____: \
	switch(LEX())
#define CASE(n, id) \
	case id: __##n##_##id##__:
#define GOTO(n, id) \
	goto __##n##_##id##__;

cell_t parser::read_expr2(){
	DM("read_expr2 {")

	switch(LEX()){
	case SDOT: DM("read_expr2 }") return ISYMBOL(dot);
	case DOT3: DM("read_expr2 }") return ISYMBOL(dot3);
	case EXPR_COMMENT: read_expr2(); DM("read_expr2 }") return 0; // ignore
	default: hold_token_ = true; break;
	}
	cell_t ret = read_expr();

	switch(LEX()){
	case SDOT: ret = read_line_expr(ret); break;
	case COLON: // TODO:
	default: hold_token_ = true; break;
	}
	DM("read_expr2 }")
	return ret;
}

cell_t parser::read_line_expr(cell_t ret){
	cell_t p = (ret = MAKE_PAIR(ret, null_cell));
	cell_t tmp;

	for(;;){
		switch(LEX()){
		case BL: continue;

		case EOB:
		case NEWLINE:
		case RP:
		case RBR:
		case RCBR:
			hold_token_ = true;
			return ret;

		default:
			hold_token_ = true;
			tmp = read_expr2();
			if(tmp)
				p = (reinterpret_cast<pair_cell*>(p)->cdr = MAKE_PAIR(tmp, null_cell));
			continue;
		}
	}
	return ret;
}

#define DM_EOB(x) if(end == EOB){DM(x)}
cell_t parser::read_expr_list(cell_t ret, char end){
	DM("read_expr_list")
	cell_t p = ret;
	cell_t tmp;

	DM_EOB("oops 1")
	for(;;){
		DM_EOB("oops 2")
		eat_delim_ = true;
		token_ = LEX();
		eat_delim_ = false;
		if(token_ == end)
			break;

		DM_EOB("oops 3")
		hold_token_ = true;
		tmp = read_expr2();
		if(tmp)
			p = ((ret ? reinterpret_cast<pair_cell*>(p)->cdr : ret) = MAKE_PAIR(tmp, null_cell));
		DM_EOB("oops 4")

		token_ = LEX();
		if(token_ == end)
			break;
		else{
			DM_EOB("oops 5")
			switch(token_){
			case EOB: throw "unfinished expr list";
			case BL:
			case NEWLINE: continue;
			default: throw "no expr list delim";
			}
		}
	}
	DM_EOB("oops 6")
	return ret ? ret : null_cell;
}

cell_t parser::read_expr(){
	DM("read_expr")
	cell_t head, key, first;

	switch(LEX()){
	case CONSTANT: return value_;
	case SYMBOL:
		head = value_;
		if(LEX() == COLON){
			return head; // TODO: symbol_to_keyword(head);
		}
		hold_token_ = true;
		break;

		// switch(LEX()){
		// case COLON: return 0; // TODO: symbol_to_keyword(head);
		// case LP: GOTO(Acell, LP)
		// case DOT: GOTO(Acell, DOT)
		// case BACKSLASH: GOTO(Acell, BACKSLASH)
		// case EX: GOTO(Acell, EX)
		// case LBR: GOTO(Acell, LBR)
		// default: hold_token_ = true; return head;// throw "illegal character after SYMBOL";
		// }

	case EOB:
	case BL:
	case NEWLINE:
	case RP:
	case RBR:
	case RCBR:
		throw "unexpected header in an expr";

	case LP: head = read_expr_list(0, RP); break;
	case VECTOR: return read_expr_list(MAKE_PAIR(ISYMBOL(vector), null_cell), RP);
	case LCBR: return read_expr_list(MAKE_PAIR(ISYMBOL(table), null_cell), RCBR);

	case QUOTE: return MAKE_LIST2(ISYMBOL(quote), read_expr());
	case BACKQUOTE: return MAKE_LIST2(ISYMBOL(quasiquote), read_expr());
	case UNQUOTE: return MAKE_LIST2(ISYMBOL(unquote), read_expr());
	case UNQUOTE_SPLICING: return MAKE_LIST2(ISYMBOL(unquote_splicing), read_expr());

	case DOT: head = ISYMBOL(empty); GOTO(Acell, DOT)
	case BACKSLASH: head = ISYMBOL(empty); GOTO(Acell, BACKSLASH)
	case EX: head = ISYMBOL(empty); GOTO(Acell, EX)
	case LBR: head = ISYMBOL(empty); GOTO(Acell, LBR)

	case AT:
		head = ISYMBOL(self);
		if(LEX() == SYMBOL){
			first = ISYMBOL(__index);
			GOTO(Key, SYMBOL)
		}
		hold_token_ = true;
		break;
	}

	for(;;){
		SWITCH_LEX(Acell){
		CASE(Acell, EOB)
		CASE(Acell, BL)
		CASE(Acell, NEWLINE)
		CASE(Acell, RP)
		CASE(Acell, RBR)
		CASE(Acell, SDOT)
			hold_token_ = true;
			return head;

		CASE(Acell, LP) head = MAKE_PAIR(head, read_expr_list(0, RP)); continue;

		CASE(Acell, DOT) first = ISYMBOL(__index); break;
		CASE(Acell, BACKSLASH) first = ISYMBOL(backslash); break;
		CASE(Acell, EX) head = MAKE_PAIR(head, null_cell); continue;
		CASE(Acell, LBR) first = ISYMBOL(__index); GOTO(Key, LBR)

		default: throw "illegal character of an acell";
		}

		SWITCH_LEX(Key){
		CASE(Key, EOB) throw "unfinished Key";
		CASE(Key, SYMBOL) key = MAKE_LIST2(ISYMBOL(quote), value_); head = MAKE_LIST3(first, head, key); break;
		CASE(Key, LBR) key = read_expr_list(0, RBR); head = MAKE_PAIR(first, MAKE_PAIR(head, key)); break;
		default: throw "illegal character of a key";
		}
	}
	return head;
}


#undef DEFINE_FIXED_SYMBOL
#define DEFINE_FIXED_SYMBOL(name, literal) \
	symbol_##name##_ = make_symbol(literal, literal + sizeof(literal));

static void* default_allocator(void* p, uint_t size){
	return realloc(p, size);
}

std::ostream& operator<<(std::ostream& os, const symbol_cell& s){
	DM(s.value<<" -> "<<"hash: "<<s.hash)
	return os;
}

void print_cell(cell_t p){
	switch(p){
	case null_cell: std::cout<<"'()"; return;
	case false_cell: std::cout<<"#f"; return;
	case true_cell: std::cout<<"#t"; return;
	default: break;
	}

	switch(reinterpret_cast<cell*>(p)->flag){
	case unknown_type:
	case number_type:
		break;
	case pair_type:
		{
			pair_cell* pair = reinterpret_cast<pair_cell*>(p);
			switch(reinterpret_cast<cell*>(pair->car)->flag){ // TODO: 这个取flag方法在CONSTANT时会有问题
			case symbol_type:
				for(;;){
					if(pair->car == ISYMBOL(quote))
						std::cout << '\'';
					else if(pair->car == ISYMBOL(unquote))
						std::cout << ',';
					else if(pair->car == ISYMBOL(unquote_splicing))
						std::cout << ",@";
					else if(pair->car == ISYMBOL(quasiquote))
						std::cout << '`';
					else if(pair->car == ISYMBOL(vector)){
						std::cout << "#";
						print_cell(pair->cdr);
						return;
					}
					else
						break;
					print_cell(reinterpret_cast<pair_cell*>(pair->cdr)->car);
					return;
				}
			default:
				std::cout << '(';
				print_cell(pair->car);
				while(pair->cdr != null_cell){
					pair = reinterpret_cast<pair_cell*>(pair->cdr);
					std::cout << ' ';
					print_cell(pair->car);
				}
				std::cout << ')';
				break;
			}
		}
		break;
	case string_type:
		std::cout << '\"' <<reinterpret_cast<string_cell*>(p)->value << '\"';
		break;
	case macro_type:

	case fixnum_type:
		break;
	case symbol_type:
		if(p == ISYMBOL(empty))
			std::cout << "||";
		else
			std::cout << reinterpret_cast<symbol_cell*>(p)->value;
		break;
	default:
		break;
	}
}

parser::parser()
	: allocator_(default_allocator)
{
	DEFINE_FIXED_SYMBOL(quote, "quote")
	DEFINE_FIXED_SYMBOL(quasiquote, "quasiquote")
	DEFINE_FIXED_SYMBOL(unquote, "unquote")
	DEFINE_FIXED_SYMBOL(unquote_splicing, "unquote-splicing")
	DEFINE_FIXED_SYMBOL(dot3, "...")
	DEFINE_FIXED_SYMBOL(dot, ".")
	DEFINE_FIXED_SYMBOL(backslash, "\\")
	DEFINE_FIXED_SYMBOL(empty, "")
	DEFINE_FIXED_SYMBOL(self, "self")
	DEFINE_FIXED_SYMBOL(vector, "vector")
	DEFINE_FIXED_SYMBOL(table, "table")
	DEFINE_FIXED_SYMBOL(__index, "__index")
}

cell_t parser::make_symbol(iter_type first, iter_type last){
	uint_t size = last - first;
	symbol_cell cell = {symbol_type, hash_range(0, first, last), size, first};
	auto it = symbols_.find(cell);
	if(it == symbols_.end()){
		auto p = reinterpret_cast<char*>(allocator_(nullptr, size + 1));
		memcpy(p, first, size);
		p[size] = '\0';
		cell.value = p;
		it = symbols_.insert(it, cell);
	}
	return reinterpret_cast<cell_t>(&*it);
}

void parser::parse(iter_type begin, iter_type end, std::ostream& os){
	begin_ = begin;
	end_ = end;
	line_ = 1;
	hold_token_ = false;
	eat_delim_ = false;

	try{
		cell_t expr = read_expr_list(0, EOB);
		std::cout << "expr["<<expr<<"] = ";
		print_cell(expr);
		DM("")
	}
	catch(const char* s){
		auto b = begin_;
		while(b < end_ && *b != '\n') ++b;
		std::string rest(begin_, b);
		DM("[line: "<<line_<<"] "<<s<<" near \""<<rest<<"\"")
		std::cout << '[';
		for(auto c : rest){
			std::cout << std::hex << (int)c << ' ';
		}
		std::cout << ']' << std::endl;
	}
}

} // end_ namespace
