/*
 *  Copyright 2006-2012 Adrian Thurston <thurston@complang.org>
 */

/*  This file is part of Colm.
 *
 *  Colm is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Colm is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Colm; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#ifndef _PARSETREE_H
#define _PARSETREE_H

#include <iostream>
#include <string.h>
#include "global.h"
#include "avlmap.h"
#include "bstmap.h"
#include "bstset.h"
#include "vector.h"
#include "dlist.h"
#include "dlistval.h"
#include "dlistmel.h"
#include "astring.h"
#include "bytecode.h"
#include "avlbasic.h"
#include "fsmrun.h"

/* Operators that are represented with single symbol characters. */
#define OP_DoubleEql 'e'
#define OP_NotEql 'q'
#define OP_LessEql 'l'
#define OP_GrtrEql 'g'
#define OP_LogicalAnd 'a'
#define OP_LogicalOr 'o'
#define OP_Deref 'd'

#if SIZEOF_LONG != 4 && SIZEOF_LONG != 8 
	#error "SIZEOF_LONG contained an unexpected value"
#endif

struct NameInst;
struct FsmGraph;
struct RedFsm;
struct _FsmRun;
struct ObjectDef;
struct ElementOf;
struct UniqueType;
struct ObjField;
struct TransBlock;
struct CodeBlock;
struct PdaLiteral;
struct TypeAlias;
typedef struct _PdaRun PdaRun;

/* 
 * Code Vector
 */
struct CodeVect : public Vector<Code>
{
	void appendHalf( Half half )
	{
		/* not optimal. */
		append( half & 0xff );
		append( (half>>8) & 0xff );
	}
	
	void appendWord( Word word )
	{
		/* not optimal. */
		append( word & 0xff );
		append( (word>>8) & 0xff );
		append( (word>>16) & 0xff );
		append( (word>>24) & 0xff );
		#if SIZEOF_LONG == 8
		append( (word>>32) & 0xff );
		append( (word>>40) & 0xff );
		append( (word>>48) & 0xff );
		append( (word>>56) & 0xff );
		#endif
	}

	void setHalf( long pos, Half half )
	{
		/* not optimal. */
		data[pos] = half & 0xff;
		data[pos+1] = (half>>8) & 0xff;
	}
	
	void insertHalf( long pos, Half half )
	{
		/* not optimal. */
		insert( pos, half & 0xff );
		insert( pos+1, (half>>8) & 0xff );
	}

	void insertWord( long pos, Word word )
	{
		/* not at all optimal. */
		insert( pos, word & 0xff );
		insert( pos+1, (word>>8) & 0xff );
		insert( pos+2, (word>>16) & 0xff );
		insert( pos+3, (word>>24) & 0xff );
		#if SIZEOF_LONG == 8
		insert( pos+4, (word>>32) & 0xff );
		insert( pos+5, (word>>40) & 0xff );
		insert( pos+6, (word>>48) & 0xff );
		insert( pos+7, (word>>56) & 0xff );
		#endif
	}
	
	void insertTree( long pos, Tree *tree )
		{ insertWord( pos, (Word) tree ); }
};



/* Types of builtin machines. */
enum BuiltinMachine
{
	BT_Any,
	BT_Ascii,
	BT_Extend,
	BT_Alpha,
	BT_Digit,
	BT_Alnum,
	BT_Lower,
	BT_Upper,
	BT_Cntrl,
	BT_Graph,
	BT_Print,
	BT_Punct,
	BT_Space,
	BT_Xdigit,
	BT_Lambda,
	BT_Empty
};

typedef BstSet<char> CharSet;
typedef Vector<unsigned char> UnsignedCharVect;


struct Compiler;
struct TypeRef;

/* Leaf type. */
struct Literal;

/* Tree nodes. */

struct Term;
struct FactorWithAug;
struct FactorWithRep;
struct FactorWithNeg;
struct Factor;
struct Expression;
struct Join;
struct JoinOrLm;
struct TokenRegion;
struct Namespace;
struct Context;
struct TokenDef;
struct TokenDefListReg;
struct TokenDefListNs;
struct Range;
struct LangEl;

/* Type of augmentation. Describes locations in the machine. */
enum AugType
{
	/* Transition actions/priorities. */
	at_start,
	at_all,
	at_finish,
	at_leave,

	/* Global error actions. */
	at_start_gbl_error,
	at_all_gbl_error,
	at_final_gbl_error,
	at_not_start_gbl_error,
	at_not_final_gbl_error,
	at_middle_gbl_error,

	/* Local error actions. */
	at_start_local_error,
	at_all_local_error,
	at_final_local_error,
	at_not_start_local_error,
	at_not_final_local_error,
	at_middle_local_error,
	
	/* To State Action embedding. */
	at_start_to_state,
	at_all_to_state,
	at_final_to_state,
	at_not_start_to_state,
	at_not_final_to_state,
	at_middle_to_state,

	/* From State Action embedding. */
	at_start_from_state,
	at_all_from_state,
	at_final_from_state,
	at_not_start_from_state,
	at_not_final_from_state,
	at_middle_from_state,

	/* EOF Action embedding. */
	at_start_eof,
	at_all_eof,
	at_final_eof,
	at_not_start_eof,
	at_not_final_eof,
	at_middle_eof
};

/* IMPORTANT: These must follow the same order as the state augs in AugType
 * since we will be using this to compose AugType. */
enum StateAugType
{
	sat_start = 0,
	sat_all,
	sat_final,
	sat_not_start,
	sat_not_final,
	sat_middle
};

struct Action;
struct PriorDesc;
struct RegExpr;
struct ReItem;
struct ReOrBlock;
struct ReOrItem;
struct ExplicitMachine;
struct InlineItem;
struct InlineList;

/* Reference to a named state. */
typedef Vector<String> NameRef;
typedef Vector<NameRef*> NameRefList;
typedef Vector<NameInst*> NameTargList;

/* Structure for storing location of epsilon transitons. */
struct EpsilonLink
{
	EpsilonLink( const InputLoc &loc, NameRef &target )
		: loc(loc), target(target) { }

	InputLoc loc;
	NameRef target;
};

struct Label
{
	Label( const InputLoc &loc, const String &data, ObjField *objField )
		: loc(loc), data(data), objField(objField) { }

	InputLoc loc;
	String data;
	ObjField *objField;
};

/* Structure represents an action assigned to some FactorWithAug node. The
 * factor with aug will keep an array of these. */
struct ParserAction
{
	ParserAction( const InputLoc &loc, AugType type, int localErrKey, Action *action )
		: loc(loc), type(type), localErrKey(localErrKey), action(action) { }

	InputLoc loc;
	AugType type;
	int localErrKey;
	Action *action;
};

struct Token
{
	String data;
	InputLoc loc;
};

void prepareLitString( String &result, bool &caseInsensitive, 
		const String &srcString, const InputLoc &loc );

std::ostream &operator<<(std::ostream &out, const Token &token );

typedef AvlMap< String, TokenDef*, CmpStr > LiteralDict;
typedef AvlMapEl< String, TokenDef* > LiteralDictEl;

/* Store the value and type of a priority augmentation. */
struct PriorityAug
{
	PriorityAug( AugType type, int priorKey, int priorValue ) :
		type(type), priorKey(priorKey), priorValue(priorValue) { }

	AugType type;
	int priorKey;
	int priorValue;
};

/*
 * A Variable Definition
 */
struct VarDef
{
	VarDef( const String &name, JoinOrLm *joinOrLm )
		: name(name), joinOrLm(joinOrLm) { }
	
	/* Parse tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( const InputLoc &loc, Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	String name;
	JoinOrLm *joinOrLm;
};

typedef Vector<String> StringVect;
typedef CmpTable<String, CmpStr> CmpStrVect;

struct NamespaceQual
{
	NamespaceQual( Namespace *declInNspace, TokenRegion *declInRegion ) : 
		cachedNspaceQual(0), declInNspace(declInNspace) {}

	Namespace *cachedNspaceQual;
	Namespace *declInNspace;

	StringVect qualNames;

	Namespace *searchFrom( Namespace *from, StringVect::Iter &qualPart );
	Namespace *getQual( Compiler *pd );
};

struct ReCapture
{
	ReCapture( Action *markEnter, Action *markLeave, ObjField *objField )
		: markEnter(markEnter), markLeave(markLeave), objField(objField)  {}

	Action *markEnter;
	Action *markLeave;
	ObjField *objField;
};

typedef Vector<Context*> ContextVect;

struct Context
{
	Context( InputLoc &loc, LangEl *lel )
	:
		loc(loc),
		lel(lel)
	{}

	InputLoc loc;
	LangEl *lel;

	ObjectDef *contextObjDef;
};

typedef Vector<ReCapture> ReCaptureVect;

struct TokenDefPtr1
{
	TokenDef *prev, *next;
};

struct TokenDefPtr2
{
	TokenDef *prev, *next;
};

struct TokenDef
:
	public TokenDefPtr1, 
	public TokenDefPtr2
{
	TokenDef( const String &name, const String &literal, bool isLiteral, bool ignore,
		Join *join, CodeBlock *codeBlock, InputLoc &semiLoc, 
		int longestMatchId, Namespace *nspace, TokenRegion *tokenRegion,
		ReCaptureVect *pReCaptureVect, ObjectDef *objectDef, Context *contextIn )
	: 
		name(name), literal(literal), isLiteral(isLiteral), ignore(ignore), join(join), action(0),
		codeBlock(codeBlock), tdLangEl(0), semiLoc(semiLoc), 
		longestMatchId(longestMatchId), inLmSelect(false), 
		nspace(nspace), tokenRegion(tokenRegion), objectDef(objectDef),
		contextIn(contextIn),
		dupOf(0), noPostIgnore(false), noPreIgnore(false), isZero(false)
	{
		if ( pReCaptureVect != 0 )
			reCaptureVect = *pReCaptureVect;
	}

	InputLoc getLoc();
	
	String name;
	String literal;
	bool isLiteral;
	bool ignore;
	Join *join;
	Action *action;
	CodeBlock *codeBlock;
	LangEl *tdLangEl;
	InputLoc semiLoc;

	Action *setActId;
	Action *actOnLast;
	Action *actOnNext;
	Action *actLagBehind;
	int longestMatchId;
	bool inLmSelect;
	Namespace *nspace;
	TokenRegion *tokenRegion;
	ReCaptureVect reCaptureVect;
	ObjectDef *objectDef;
	Context *contextIn;

	TokenDef *dupOf;
	bool noPostIgnore;
	bool noPreIgnore;
	bool isZero;
};

struct LelDefList;

struct NtDef
{
	NtDef( const String &name, Namespace *nspace,
		LelDefList *defList, ObjectDef *objectDef,
		Context *contextIn, bool reduceFirst )
	: 
		name(name), 
		nspace(nspace),
		defList(defList),
		objectDef(objectDef),
		contextIn(contextIn),
		reduceFirst(reduceFirst)
	{}

	String name;
	Namespace *nspace;
	LelDefList *defList;
	ObjectDef *objectDef;
	Context *contextIn;
	bool reduceFirst;

	NtDef *prev, *next;
};

struct NtDefList : DList<NtDef> {};

/* Declare a new type so that ptreetypes.h need not include dlist.h. */
struct TokenDefListReg : DListMel<TokenDef, TokenDefPtr1> {};
struct TokenDefListNs : DListMel<TokenDef, TokenDefPtr2> {};

struct ContextDef
{
	ContextDef( const String &name, Context *context, Namespace *nspace )
		: name(name), context(context), nspace(nspace) {}

	String name;
	Context *context;
	Namespace *nspace;

	ContextDef *prev, *next;
};

struct ContextDefList : DList<ContextDef> {};

struct TypeMapEl
	: public AvlTreeEl<TypeMapEl>
{
	enum Type
	{
		TypeAliasType = 1,
		LangElType
	};

	const String &getKey() { return key; }

	TypeMapEl( const String &key, TypeRef *typeRef )
		: type(TypeAliasType), key(key), value(0), typeRef(typeRef) {}

	TypeMapEl( const String &key, LangEl *value )
		: type(LangElType), key(key), value(value), typeRef(0) {}


	Type type;
	String key;
	LangEl *value;
	TypeRef *typeRef;
	
	TypeMapEl *prev, *next;
};

/* Symbol Map. */
typedef AvlTree< TypeMapEl, String, CmpStr > TypeMap;

typedef Vector<TokenRegion*> RegionVect;

struct TokenRegion
{
	/* Construct with a list of joins */
	TokenRegion( const InputLoc &loc, const String &name, int id, 
			TokenRegion *parentRegion ) : 
		loc(loc), name(name), id(id),
		lmSwitchHandlesError(false), regionNameInst(0),
		parentRegion(parentRegion), defaultTokenDef(0),
		preEofBlock(0), 
		ignoreOnlyRegion(0), tokenOnlyRegion(0), ciRegion(0),
		wasEmpty(false), 
		isFullRegion(false),
		isIgnoreOnly(false), 
		isTokenOnly(false), 
		isCiOnly(false),
		ciLel(0),
		derivedFrom(0)
	{ }

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );
	void runLongestMatch( Compiler *pd, FsmGraph *graph );
	void transferScannerLeavingActions( FsmGraph *graph );
	Action *newAction( Compiler *pd, const InputLoc &loc, const String &name, 
			InlineList *inlineList );
	void makeActions( Compiler *pd );
	void findName( Compiler *pd );
	void restart( FsmGraph *graph, FsmTrans *trans );

	InputLoc loc;
	TokenDefListReg tokenDefList;
	String name;
	int id;

	Action *lmActSelect;
	bool lmSwitchHandlesError;

	/* This gets saved off during the name walk. Can save it off because token
	 * regions are referenced once only. */
	NameInst *regionNameInst;

	TokenRegion *parentRegion;
	RegionVect childRegions;

	TokenDef *defaultTokenDef;

	CodeBlock *preEofBlock;

	/* Dupe of the region, containing only the ignore tokens. */
	TokenRegion *ignoreOnlyRegion;
	TokenRegion *tokenOnlyRegion;
	TokenRegion *ciRegion;

	/* We alway init empty scanners with a single token. If we had to do this
	 * then wasEmpty is true. */
	bool wasEmpty;

	bool isFullRegion;
	bool isIgnoreOnly;
	bool isTokenOnly;
	bool isCiOnly;

	LangEl *ciLel;
	TokenRegion *derivedFrom;

	TokenRegion *next, *prev;
};

typedef DList<TokenRegion> RegionList;
typedef BstSet< TokenRegion*, CmpOrd<TokenRegion*> > RegionSet;

typedef Vector<Namespace*> NamespaceVect;

struct GenericType 
	: public DListEl<GenericType>
{
	GenericType( const String &name, long typeId, long id, 
			LangEl *langEl, TypeRef *typeArg )
	:
		name(name), typeId(typeId), id(id), langEl(langEl),
		typeArg(typeArg), keyTypeArg(0), 
		utArg(0), keyUT(0),
		objDef(0)
	{}

	const String &getKey() const 
		{ return name; };

	void declare( Compiler *pd, Namespace *nspace );

	String name;
	long typeId;
	long id;
	LangEl *langEl;
	TypeRef *typeArg;
	TypeRef *keyTypeArg;
	UniqueType *utArg;
	UniqueType *keyUT;

	ObjectDef *objDef;
};

typedef DList<GenericType> GenericList;

typedef struct _UserIter UserIter;
typedef AvlMap<String, UserIter*, CmpStr> UserIterMap;
typedef AvlMapEl<String, UserIter*> UserIterMapEl;

/* Graph dictionary. */
struct GraphDictEl 
:
	public AvlTreeEl<GraphDictEl>,
	public DListEl<GraphDictEl>
{
	GraphDictEl( const String &key ) 
		: key(key), value(0), isInstance(false) { }
	GraphDictEl( const String &key, VarDef *value ) 
		: key(key), value(value), isInstance(false) { }

	const String &getKey() { return key; }

	String key;
	VarDef *value;
	bool isInstance;

	/* Location info of graph definition. Points to variable name of assignment. */
	InputLoc loc;
};

typedef AvlTree<GraphDictEl, String, CmpStr> GraphDict;
typedef DList<GraphDictEl> GraphList;

struct TypeAlias
{
	TypeAlias( const InputLoc &loc, Namespace *nspace, 
			const String &name, TypeRef *typeRef )
	:
		loc(loc),
		nspace(nspace),
		name(name),
		typeRef(typeRef)
	{}

	InputLoc loc;
	Namespace *nspace;
	String name;
	TypeRef *typeRef;

	TypeAlias *prev, *next;
};

typedef DList<TypeAlias> TypeAliasList;

struct Namespace
{
	/* Construct with a list of joins */
	Namespace( const InputLoc &loc, const String &name, int id, 
			Namespace *parentNamespace ) : 
		loc(loc), name(name), id(id),
		parentNamespace(parentNamespace) { }

	/* Tree traversal. */
	Namespace *findNamespace( const String &name );

	InputLoc loc;
	String name;
	int id;

	/* Literal patterns and the dictionary mapping literals to the underlying
	 * tokens. */
	LiteralDict literalDict;

	/* List of tokens defs in the namespace. */
	TokenDefListNs tokenDefList;

	/* List of nonterminal defs in the namespace. */
	NtDefList ntDefList;

	/* List of context definitions for encapsulating the data of a parser. */
	ContextDefList contextDefList;

	/* Dictionary of symbols within the region. */
	TypeMap typeMap;
	GenericList genericList;

	/* Dictionary of graphs. Both instances and non-instances go here. */
	GraphDict graphDict;

	/* regular language definitions. */
	GraphDict rlMap;

	TypeAliasList typeAliasList;

	Namespace *parentNamespace;
	NamespaceVect childNamespaces;

	Namespace *next, *prev;

	void declare( Compiler *pd );
};

typedef DList<Namespace> NamespaceList;
typedef BstSet< Namespace*, CmpOrd<Namespace*> > NamespaceSet;

/* List of Expressions. */
typedef DList<Expression> ExprList;

struct JoinOrLm
{
	enum Type {
		JoinType,
		LongestMatchType
	};

	JoinOrLm( Join *join ) : 
		join(join), type(JoinType) {}
	JoinOrLm( TokenRegion *tokenRegion ) :
		tokenRegion(tokenRegion), type(LongestMatchType) {}

	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );
	
	Join *join;
	TokenRegion *tokenRegion;
	Type type;
};

/*
 * Join
 */
struct Join
{
	/* Construct with the first expression. */
	Join( Expression *expr );

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	/* Data. */
	ExprList exprList;

	Join *context;
	Action *mark;
};

/*
 * Expression
 */
struct Expression
{
	enum Type { 
		OrType,
		IntersectType, 
		SubtractType, 
		StrongSubtractType,
		TermType, 
		BuiltinType
	};

	/* Construct with an expression on the left and a term on the right. */
	Expression( Expression *expression, Term *term, Type type ) : 
		expression(expression), term(term), 
		builtin(builtin), type(type), prev(this), next(this) { }

	/* Construct with only a term. */
	Expression( Term *term ) : 
		expression(0), term(term), builtin(builtin), 
		type(TermType) , prev(this), next(this) { }
	
	/* Construct with a builtin type. */
	Expression( BuiltinMachine builtin ) : 
		expression(0), term(0), builtin(builtin), 
		type(BuiltinType), prev(this), next(this) { }

	~Expression();

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd, bool lastInSeq = true );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	/* Node data. */
	Expression *expression;
	Term *term;
	BuiltinMachine builtin;
	Type type;

	Expression *prev, *next;
};

/*
 * Term
 */
struct Term 
{
	enum Type { 
		ConcatType, 
		RightStartType,
		RightFinishType,
		LeftType,
		FactorWithAugType
	};

	Term( Term *term, FactorWithAug *factorWithAug ) :
		term(term), factorWithAug(factorWithAug), type(ConcatType) { }

	Term( Term *term, FactorWithAug *factorWithAug, Type type ) :
		term(term), factorWithAug(factorWithAug), type(type) { }

	Term( FactorWithAug *factorWithAug ) :
		term(0), factorWithAug(factorWithAug), type(FactorWithAugType) { }
	
	~Term();

	FsmGraph *walk( Compiler *pd, bool lastInSeq = true );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	Term *term;
	FactorWithAug *factorWithAug;
	Type type;

	/* Priority descriptor for RightFinish type. */
	PriorDesc priorDescs[2];
};


/* Third level of precedence. Augmenting nodes with actions and priorities. */
struct FactorWithAug
{
	FactorWithAug( FactorWithRep *factorWithRep ) :
		priorDescs(0), factorWithRep(factorWithRep) { }
	~FactorWithAug();

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	void assignActions( Compiler *pd, FsmGraph *graph, int *actionOrd );
	void assignPriorities( FsmGraph *graph, int *priorOrd );

	void assignConditions( FsmGraph *graph );

	/* Actions and priorities assigned to the factor node. */
	Vector<ParserAction> actions;
	Vector<PriorityAug> priorityAugs;
	PriorDesc *priorDescs;
	Vector<Label> labels;
	Vector<EpsilonLink> epsilonLinks;
	Vector<ParserAction> conditions;

	FactorWithRep *factorWithRep;
};

/* Fourth level of precedence. Trailing unary operators. Provide kleen star,
 * optional and plus. */
struct FactorWithRep
{
	enum Type { 
		StarType,
		StarStarType,
		OptionalType,
		PlusType, 
		ExactType,
		MaxType,
		MinType,
		RangeType,
		FactorWithNegType
	};

	 FactorWithRep( const InputLoc &loc, FactorWithRep *factorWithRep, 
			int lowerRep, int upperRep, Type type ) :
		loc(loc), factorWithRep(factorWithRep), 
		factorWithNeg(0), lowerRep(lowerRep), 
		upperRep(upperRep), type(type) { }
	
	FactorWithRep( const InputLoc &loc, FactorWithNeg *factorWithNeg )
		: loc(loc), factorWithNeg(factorWithNeg), type(FactorWithNegType) { }

	~FactorWithRep();

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	InputLoc loc;
	FactorWithRep *factorWithRep;
	FactorWithNeg *factorWithNeg;
	int lowerRep, upperRep;
	Type type;

	/* Priority descriptor for StarStar type. */
	PriorDesc priorDescs[2];
};

/* Fifth level of precedence. Provides Negation. */
struct FactorWithNeg
{
	enum Type { 
		NegateType, 
		CharNegateType,
		FactorType
	};

	FactorWithNeg( const InputLoc &loc, FactorWithNeg *factorWithNeg, Type type) :
		loc(loc), factorWithNeg(factorWithNeg), factor(0), type(type) { }

	FactorWithNeg( const InputLoc &loc, Factor *factor ) :
		loc(loc), factorWithNeg(0), factor(factor), type(FactorType) { }

	~FactorWithNeg();

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	InputLoc loc;
	FactorWithNeg *factorWithNeg;
	Factor *factor;
	Type type;
};

/*
 * Factor
 */
struct Factor
{
	/* Language elements a factor node can be. */
	enum Type {
		LiteralType, 
		RangeType, 
		OrExprType,
		RegExprType, 
		ReferenceType,
		ParenType,
	}; 

	/* Construct with a literal fsm. */
	Factor( Literal *literal ) :
		literal(literal), type(LiteralType) { }

	/* Construct with a range. */
	Factor( Range *range ) : 
		range(range), type(RangeType) { }
	
	/* Construct with the or part of a regular expression. */
	Factor( ReItem *reItem ) :
		reItem(reItem), type(OrExprType) { }

	/* Construct with a regular expression. */
	Factor( RegExpr *regExp ) :
		regExp(regExp), type(RegExprType) { }

	/* Construct with a reference to a var def. */
	Factor( const InputLoc &loc, VarDef *varDef ) :
		loc(loc), varDef(varDef), type(ReferenceType) {}

	/* Construct with a parenthesized join. */
	Factor( Join *join ) :
		join(join), type(ParenType) {}
	
	/* Cleanup. */
	~Factor();

	/* Tree traversal. */
	FsmGraph *walk( Compiler *pd );
	void makeNameTree( Compiler *pd );
	void resolveNameRefs( Compiler *pd );

	InputLoc loc;
	Literal *literal;
	Range *range;
	ReItem *reItem;
	RegExpr *regExp;
	VarDef *varDef;
	Join *join;
	int lower, upper;
	Type type;
};

/* A range machine. Only ever composed of two literals. */
struct Range
{
	Range( Literal *lowerLit, Literal *upperLit ) 
		: lowerLit(lowerLit), upperLit(upperLit) { }

	~Range();
	FsmGraph *walk( Compiler *pd );
	bool verifyRangeFsm( FsmGraph *rangeEnd );

	Literal *lowerLit;
	Literal *upperLit;
};

/* Some literal machine. Can be a number or literal string. */
struct Literal
{
	enum LiteralType { Number, LitString };

	Literal( const InputLoc &loc, const String &literal, LiteralType type )
		: loc(loc), literal(literal), type(type) { }

	FsmGraph *walk( Compiler *pd );
	
	InputLoc loc;
	String literal;
	LiteralType type;
};

/* Regular expression. */
struct RegExpr
{
	enum RegExpType { RecurseItem, Empty };

	/* Constructors. */
	RegExpr() : 
		type(Empty), caseInsensitive(false) { }
	RegExpr(RegExpr *regExp, ReItem *item) : 
		regExp(regExp), item(item), 
		type(RecurseItem), caseInsensitive(false) { }

	~RegExpr();
	FsmGraph *walk( Compiler *pd, RegExpr *rootRegex );

	RegExpr *regExp;
	ReItem *item;
	RegExpType type;
	bool caseInsensitive;
};

/* An item in a regular expression. */
struct ReItem
{
	enum ReItemType { Data, Dot, OrBlock, NegOrBlock };
	
	ReItem( const InputLoc &loc, const String &data ) 
		: loc(loc), data(data), star(false), type(Data) { }
	ReItem( const InputLoc &loc, ReItemType type )
		: loc(loc), star(false), type(type) { }
	ReItem( const InputLoc &loc, ReOrBlock *orBlock, ReItemType type )
		: loc(loc), orBlock(orBlock), star(false), type(type) { }

	~ReItem();
	FsmGraph *walk( Compiler *pd, RegExpr *rootRegex );

	InputLoc loc;
	String data;
	ReOrBlock *orBlock;
	bool star;
	ReItemType type;
};

/* An or block item. */
struct ReOrBlock
{
	enum ReOrBlockType { RecurseItem, Empty };

	/* Constructors. */
	ReOrBlock()
		: type(Empty) { }
	ReOrBlock(ReOrBlock *orBlock, ReOrItem *item)
		: orBlock(orBlock), item(item), type(RecurseItem) { }

	~ReOrBlock();
	FsmGraph *walk( Compiler *pd, RegExpr *rootRegex );
	
	ReOrBlock *orBlock;
	ReOrItem *item;
	ReOrBlockType type;
};

/* An item in an or block. */
struct ReOrItem
{
	enum ReOrItemType { Data, Range };

	ReOrItem( const InputLoc &loc, const String &data ) 
		: loc(loc), data(data), type(Data) {}
	ReOrItem( const InputLoc &loc, char lower, char upper )
		: loc(loc), lower(lower), upper(upper), type(Range) { }

	FsmGraph *walk( Compiler *pd, RegExpr *rootRegex );

	InputLoc loc;
	String data;
	char lower;
	char upper;
	ReOrItemType type;
};


/*
 * Inline code tree
 */
struct InlineList;
struct InlineItem
{
	enum Type 
	{
		Text, 
		LmSwitch, 
		LmSetActId, 
		LmSetTokEnd, 
		LmOnLast, 
		LmOnNext,
		LmOnLagBehind, 
		LmInitAct, 
		LmInitTokStart, 
		LmSetTokStart 
	};

	InlineItem( const InputLoc &loc, const String &data, Type type ) : 
		loc(loc), data(data), nameRef(0), children(0), type(type) { }

	InlineItem( const InputLoc &loc, NameRef *nameRef, Type type ) : 
		loc(loc), nameRef(nameRef), children(0), type(type) { }

	InlineItem( const InputLoc &loc, TokenRegion *tokenRegion, 
		TokenDef *longestMatchPart, Type type ) : loc(loc),
		nameRef(0), children(0), tokenRegion(tokenRegion),
		longestMatchPart(longestMatchPart), type(type) { } 

	InlineItem( const InputLoc &loc, NameInst *nameTarg, Type type ) : 
		loc(loc), nameRef(0), nameTarg(nameTarg), children(0),
		type(type) { }

	InlineItem( const InputLoc &loc, Type type ) : 
		loc(loc), nameRef(0), children(0), type(type) { }
	
	InputLoc loc;
	String data;
	NameRef *nameRef;
	NameInst *nameTarg;
	InlineList *children;
	TokenRegion *tokenRegion;
	TokenDef *longestMatchPart;
	Type type;

	InlineItem *prev, *next;
};

/* Normally this would be atypedef, but that would entail including DList from
 * ptreetypes, which should be just typedef forwards. */
struct InlineList : public DList<InlineItem> { };

struct ProdEl;
struct LangVarRef;
struct ObjField;

struct PatternItem
{
	enum Type { 
		FactorType,
		InputText
	};

	PatternItem( const InputLoc &loc, const String &data, Type type ) : 
			loc(loc), factor(0), data(data), type(type), region(0), 
			varRef(0), bindId(0) {}

	PatternItem( const InputLoc &loc, ProdEl *factor, Type type ) : 
			loc(loc), factor(factor), type(type), region(0), 
			varRef(0), bindId(0) {}

	InputLoc loc;
	ProdEl *factor;
	String data;
	Type type;
	TokenRegion *region;
	LangVarRef *varRef;
	long bindId;

	PatternItem *prev, *next;
};

struct LangExpr;
typedef DList<PatternItem> PatternItemList;

struct ReplItem
{
	enum Type { 
		InputText, 
		ExprType,
		FactorType
	};

	ReplItem( const InputLoc &loc, Type type, const String &data ) : 
		loc(loc), type(type), data(data), expr(0), bindId(0) {}

	ReplItem( const InputLoc &loc, Type type, LangExpr *expr ) : 
		loc(loc), type(type), expr(expr), bindId(0) {}

	ReplItem( const InputLoc &loc, Type type, ProdEl *factor ) : 
		loc(loc), type(type), expr(expr), factor(factor), bindId(0) {}

	InputLoc loc;
	Type type;
	String data;
	LangExpr *expr;
	LangEl *langEl;
	ProdEl *factor;
	long bindId;

	ReplItem *prev, *next;
};

typedef DList<ReplItem> ReplItemList;


struct Pattern
{
	Pattern( const InputLoc &loc, Namespace *nspace, TokenRegion *region, 
			PatternItemList *list, int patRepId ) : 
		loc(loc), nspace(nspace), region(region), list(list), patRepId(patRepId), 
		langEl(0), pdaRun(0), nextBindId(1) {}
	
	InputLoc loc;
	Namespace *nspace;
	TokenRegion *region;
	PatternItemList *list;
	long patRepId;
	LangEl *langEl;
	PdaRun *pdaRun;
	long nextBindId;

	Pattern *prev, *next;
};

typedef DList<Pattern> PatternList;

struct Replacement
{
	Replacement( const InputLoc &loc, Namespace *nspace, 
			TokenRegion *region, ReplItemList *list, int patRepId ) :
		loc(loc), nspace(nspace), region(region), list(list), 
		patRepId(patRepId), langEl(0), pdaRun(0), nextBindId(1), parse(true) {}

	InputLoc loc;
	Namespace *nspace;
	TokenRegion *region;
	ReplItemList *list;
	int patRepId;
	LangEl *langEl;
	PdaRun *pdaRun;
	long nextBindId;
	bool parse;

	Replacement *prev, *next;
};

typedef DList<Replacement> ReplList;

struct ParserText
{
	ParserText( const InputLoc &loc, Namespace *nspace, 
			TokenRegion *region, ReplItemList *list ) :
		loc(loc), nspace(nspace), region(region), list(list), 
		langEl(0), pdaRun(0), nextBindId(1), parse(true) {}

	InputLoc loc;
	Namespace *nspace;
	TokenRegion *region;
	ReplItemList *list;
	LangEl *langEl;
	PdaRun *pdaRun;
	long nextBindId;
	bool parse;

	ParserText *prev, *next;
};

typedef DList<ParserText> ParserTextList;

struct Function;

struct IterDef
{
	enum Type { Tree, Child, RevChild, Repeat, RevRepeat, User };

	IterDef( Type type, Function *func );
	IterDef( Type type );

	Type type;

	Function *func;
	bool useFuncId;
	bool useSearchUT;

	Code inCreateWV;
	Code inCreateWC;
	Code inDestroy;
	Code inAdvance;

	Code inGetCurR;
	Code inGetCurWC;
	Code inSetCurWC;

	Code inRefFromCur;
};

struct CmpIterDef
{
	static int compare( const IterDef &id1, const IterDef &id2 )
	{
		if ( id1.type < id2.type )
			return -1;
		else if ( id1.type > id2.type )
			return 1;
		else if ( id1.type == IterDef::User ) {
			if ( id1.func < id2.func )
				return -1;
			else if ( id1.func > id2.func )
				return 1;
		}
			
		return 0;
	}
};

typedef AvlSet<IterDef, CmpIterDef> IterDefSet;
typedef AvlSetEl<IterDef> IterDefSetEl;


/*
 * Unique Types.
 */

/*
 *  type_ref -> qualified_name
 *  type_ref -> '*' type_ref
 *  type_ref -> '&' type_ref
 *  type_ref -> list type_ref type_ref
 *  type_ref -> map type_ref type_ref
 *  type_ref -> vector type_ref
 *  type_ref -> parser type_ref
 *  type_ref -> iter_tree type_ref
 *  type_ref -> iter_child type_ref
 *  type_ref -> iter_revchild type_ref
 *  type_ref -> iter_repeat type_ref
 *  type_ref -> iter_revrepeat type_ref
 *  type_ref -> iter_user type_ref
 *
 *  type -> nil
 *  type -> def term 
 *  type -> def nonterm
 *  type -> '*' type
 *  type -> '&' type
 *  type -> list type 
 *  type -> map type type
 *  type -> vector type
 *  type -> parser type
 *  type -> iter_tree type
 *  type -> iter_child type
 *  type -> iter_revchild type
 *  type -> iter_repeat type
 *  type -> iter_revrepeat type
 *  type -> iter_user type
 */

struct UniqueType : public AvlTreeEl<UniqueType>
{
	UniqueType( int typeId ) :
		typeId(typeId), 
		langEl(0), 
		iterDef(0) {}

	UniqueType( int typeId, LangEl *langEl ) :
		typeId(typeId),
		langEl(langEl),
		iterDef(0) {}

	UniqueType( int typeId, IterDef *iterDef ) :
		typeId(typeId),
		langEl(langEl),
		iterDef(iterDef) {}

	int typeId;
	LangEl *langEl;
	IterDef *iterDef;
};

struct CmpUniqueType
{
	static int compare( const UniqueType &ut1, const UniqueType &ut2 );
};

typedef AvlBasic< UniqueType, CmpUniqueType > UniqueTypeMap;

enum RepeatType {
	RepeatNone = 1,
	RepeatRepeat,
	RepeatList,
	RepeatOpt,
};

/*
 * Repeat types.
 */

struct UniqueRepeat
	: public AvlTreeEl<UniqueRepeat>
{
	UniqueRepeat( RepeatType repeatType, LangEl *langEl ) :
		repeatType(repeatType),
		langEl(langEl), declLangEl(0) {}

	RepeatType repeatType;
	LangEl *langEl;
	LangEl *declLangEl;
};

struct CmpUniqueRepeat
{
	static int compare( const UniqueRepeat &ut1, const UniqueRepeat &ut2 );
};

typedef AvlBasic< UniqueRepeat, CmpUniqueRepeat > UniqueRepeatMap;

/* 
 * Unique Map Types
 */

struct UniqueMap
	: public AvlTreeEl<UniqueMap>
{
	UniqueMap( UniqueType *key, UniqueType *value ) :
		key(key), value(value), generic(0) {}

	UniqueType *key;
	UniqueType *value;

	GenericType *generic;
};

struct CmpUniqueMap
{
	static int compare( const UniqueMap &ut1, const UniqueMap &ut2 );
};

typedef AvlBasic< UniqueMap, CmpUniqueMap > UniqueMapMap;

/* 
 * Unique List Types
 */

struct UniqueList
	: public AvlTreeEl<UniqueList>
{
	UniqueList( UniqueType *value ) :
		value(value), generic(0) {}

	UniqueType *value;
	GenericType *generic;
};

struct CmpUniqueList
{
	static int compare( const UniqueList &ut1, const UniqueList &ut2 );
};

typedef AvlBasic< UniqueList, CmpUniqueList > UniqueListMap;

/* 
 * Unique Vector Types
 */

struct UniqueVector
	: public AvlTreeEl<UniqueVector>
{
	UniqueVector( UniqueType *value ) :
		value(value), generic(0) {}

	UniqueType *value;
	GenericType *generic;
};

struct CmpUniqueVector
{
	static int compare( const UniqueVector &ut1, const UniqueVector &ut2 );
};

typedef AvlBasic< UniqueVector, CmpUniqueVector > UniqueVectorMap;

/* 
 * Unique Parser Types
 */

struct UniqueParser
	: public AvlTreeEl<UniqueParser>
{
	UniqueParser( UniqueType *parseType ) :
		parseType(parseType), generic(0) {}

	UniqueType *parseType;
	GenericType *generic;
};

struct CmpUniqueParser
{
	static int compare( const UniqueParser &ut1, const UniqueParser &ut2 );
};

typedef AvlBasic< UniqueParser, CmpUniqueParser > UniqueParserMap;

/*
 *
 */

typedef AvlMap< StringVect, int, CmpStrVect > VectorTypeIdMap;
typedef AvlMapEl< StringVect, int > VectorTypeIdMapEl;

typedef Vector<TypeRef*> TypeRefVect;

struct TypeRef
{
	enum Type
	{
		Unspecified,
		Name,
		Literal,
		Iterator,
		Map,
		List,
		Vector,
		Parser,
		Ref,
		Ptr,
	};

	/* Qualification and a type name. These require lookup. */
	TypeRef( const InputLoc &loc, NamespaceQual *nspaceQual, String typeName ) :
		type(Name), loc(loc), nspaceQual(nspaceQual), typeName(typeName), pdaLiteral(0), iterDef(0),
		typeRef1(0), typeRef2(0),
		repeatType(RepeatNone),
		nspace(0), uniqueType(0), searchUniqueType(0), generic(0) {}

	/* Qualification and a type name. These require lookup. */
	TypeRef( const InputLoc &loc, NamespaceQual *nspaceQual, PdaLiteral *pdaLiteral ) :
		type(Literal), loc(loc), nspaceQual(nspaceQual), pdaLiteral(pdaLiteral), iterDef(0),
		typeRef1(0), typeRef2(0),
		repeatType(RepeatNone),
		nspace(0), uniqueType(0), searchUniqueType(0), generic(0) {}

	/* Generics. */
	TypeRef( Type type, const InputLoc &loc, NamespaceQual *nspaceQual, TypeRef *typeRef1, TypeRef *typeRef2 ) :
		type(type), loc(loc), nspaceQual(nspaceQual), pdaLiteral(0), iterDef(0),
		typeRef1(typeRef1), typeRef2(typeRef2),
		repeatType(RepeatNone),
		nspace(0), uniqueType(0), searchUniqueType(0), generic(0) {}
	
	/* Pointers and Refs. */
	TypeRef( Type type, const InputLoc &loc, TypeRef *typeRef1 ) :
		type(type), loc(loc), nspaceQual(0), pdaLiteral(0), iterDef(0),
		typeRef1(typeRef1), typeRef2(0),
		repeatType(RepeatNone),
		nspace(0), uniqueType(0), searchUniqueType(0), generic(0) {}

	/* Resolution not needed. */

	/* Iterator definition. */
	TypeRef( const InputLoc &loc, IterDef *iterDef, UniqueType *uniqueType, 
			UniqueType *searchUniqueType ) :
		type(Iterator), loc(loc), nspaceQual(0), pdaLiteral(0), iterDef(iterDef),
		typeRef1(0), typeRef2(0),
		repeatType(RepeatNone),
		nspace(0), uniqueType(uniqueType), searchUniqueType(searchUniqueType), generic(0) {}

	/* Unique type is given directly. */
	TypeRef( const InputLoc &loc, UniqueType *uniqueType ) :
		type(Unspecified), loc(loc), nspaceQual(0), pdaLiteral(0), iterDef(0),
		typeRef1(0), typeRef2(0),
		repeatType(RepeatNone),
		nspace(0), uniqueType(uniqueType), searchUniqueType(0), generic(0) {}

	void resolveRepeat( Compiler *pd );

	UniqueType *lookupTypeName( Compiler *pd );
	UniqueType *lookupTypeLiteral( Compiler *pd );
	UniqueType *lookupTypeMap( Compiler *pd );
	UniqueType *lookupTypeList( Compiler *pd );
	UniqueType *lookupTypeVector( Compiler *pd );
	UniqueType *lookupTypeParser( Compiler *pd );
	UniqueType *lookupType( Compiler *pd );
	UniqueType *lookupTypePtr( Compiler *pd );
	UniqueType *lookupTypeRef( Compiler *pd );

	Type type;
	InputLoc loc;
	NamespaceQual *nspaceQual;
	String typeName;
	PdaLiteral *pdaLiteral;
	IterDef *iterDef;
	TypeRef *typeRef1;
	TypeRef *typeRef2;
	RepeatType repeatType;

	/* Resolved. */
	Namespace *nspace;
	UniqueType *uniqueType;
	UniqueType *searchUniqueType;
	GenericType *generic;
};

typedef DList<ObjField> ParameterList; 

struct ObjMethod
{
	ObjMethod( UniqueType *returnUT, String name, 
			int opcodeWV, int opcodeWC, int numParams, 
			UniqueType **types, ParameterList *paramList, bool isConst )
	: 
		returnUT(returnUT),
		returnTypeId(0),
		name(name), 
		opcodeWV(opcodeWV),
		opcodeWC(opcodeWC), 
		numParams(numParams),
		paramList(paramList), 
		isConst(isConst),
		funcId(0), 
		useFuncId(false),
		useCallObj(true),
		isCustom(false),
		func(0), 
		iterDef(0)
	{
		this->paramUTs = new UniqueType*[numParams];
		memcpy( this->paramUTs, types, sizeof(UniqueType*)*numParams );
	}

	UniqueType *returnUT;
	long returnTypeId;
	String name;
	long opcodeWV;
	long opcodeWC;
	long numParams;
	UniqueType **paramUTs;
	ParameterList *paramList;
	bool isConst;
	long funcId;
	bool useFuncId;
	bool useCallObj;
	bool isCustom;
	Function *func;
	IterDef *iterDef;
};

typedef AvlMap<String, ObjMethod*, CmpStr> ObjMethodMap;
typedef AvlMapEl<String, ObjMethod*> ObjMethodMapEl;

struct RhsVal { RhsVal( int prodNum, int childNum ) : prodNum(prodNum), childNum(childNum) { } int prodNum; int childNum; };

struct ObjField
{
	ObjField( const InputLoc &loc, TypeRef *typeRef, const String &name ) : 
		loc(loc), typeRef(typeRef), name(name), 
		context(0),
		pos(0), offset(0),
		beenReferenced(false),
		beenInitialized(false),
		useOffset(true),
		isConst(false), 
		isLhsEl(false), isRhsEl(false), 
		refActive(false),
		isArgv(false),
		isCustom(false),
		isParam(false),
		isRhsGet(false),
		isExport(false),
		dirtyTree(false),
		inGetR( IN_HALT ),
		inGetWC( IN_HALT ),
		inGetWV( IN_HALT ),
		inSetWC( IN_HALT ),
		inSetWV( IN_HALT )
		{}
	
	InputLoc loc;
	TypeRef *typeRef;
	String name;
	Context *context;
	long pos;
	long offset;
	bool beenReferenced;
	bool beenInitialized;
	bool useOffset;
	bool isConst;
	bool isLhsEl;
	bool isRhsEl;
	bool refActive;
	bool isArgv;
	bool isCustom;
	bool isParam;
	bool isRhsGet;
	bool isExport;
	
	/* True if some aspect of the tree has possibly been written to. This does
	 * not include attributes. This is here so we can optimize the storage of
	 * old lhs vars. If only a lhs attribute changes we don't need to preserve
	 * the original for backtracking. */
	bool dirtyTree;

	Vector<RhsVal> rhsVal;

	Code inGetR;
	Code inGetWC;
	Code inGetWV;
	Code inSetWC;
	Code inSetWV;

	ObjField *prev, *next;
};

typedef AvlMap<String, ObjField*, CmpStr> ObjFieldMap;
typedef AvlMapEl<String, ObjField*> ObjFieldMapEl;

typedef DListVal<ObjField*> ObjFieldList;

typedef DList<ObjField> ParameterList; 

struct TemplateType;

/* Tree of name scopes for an object def. All of the object fields inside this
 * tree live in one object def. This is used for scoping names in functions. */
struct ObjNameScope
{
	ObjNameScope()
		: parentScope(0), childIter(0)
	{}

	ObjFieldMap *objFieldMap;	

	ObjNameScope *parentScope;
	DList<ObjNameScope> children;

	/* For iteration after declaration. */
	ObjNameScope *childIter;

	ObjNameScope *prev, *next;
};

struct ObjectDef
{
	enum Type {
		UserType,
		FrameType,
		IterType,
		BuiltinType
	};

	ObjectDef( Type type, String name, int id )
	:
		type(type), name(name), id(id), 
		nextOffset(0), firstNonTree(0)
	{
		scope = new ObjNameScope;
		scope->objFieldMap = new ObjFieldMap;

		objFieldList = new ObjFieldList;
		objMethodMap = new ObjMethodMap();
	}

	Type type;
	String name;
	ObjFieldList *objFieldList;
	ObjMethodMap *objMethodMap;	

	/* Head of stack of name scopes. */
	ObjNameScope *scope;

	void pushScope();
	void popScope();
	void iterPushScope();
	void iterPopScope();

	long id;
	long nextOffset;
	long firstNonTree;

	void referenceField( Compiler *pd, ObjField *field );
	void initField( Compiler *pd, ObjField *field );
	void createCode( Compiler *pd, CodeVect &code );
	ObjField *checkRedecl( const String &name );
	ObjMethod *findMethod( const String &name );
	ObjField *findFieldInScope( const String &name, ObjNameScope *inScope );
	ObjField *findField( const String &name );
	void insertField( const String &name, ObjField *value );
	void resolve( Compiler *pd );
	ObjField *findFieldNum( long offset );

	long size() { return nextOffset; }
	long sizeTrees() { return firstNonTree; }
};

typedef Vector<LangExpr*> ExprVect;
typedef Vector<String> StringVect;

struct FieldInit
{
	FieldInit( const InputLoc &loc, String name, LangExpr *expr )
		: loc(loc), name(name), expr(expr) {}

	InputLoc loc;
	String name;
	LangExpr *expr;

	UniqueType *exprUT;
};

typedef Vector<FieldInit*> FieldInitVect;

struct VarRefLookup
{
	VarRefLookup( int lastPtrInQual, int firstConstPart, ObjectDef *inObject ) :
		lastPtrInQual(lastPtrInQual), 
		firstConstPart(firstConstPart),
		inObject(inObject),
		objField(0), 
		objMethod(0), 
		uniqueType(0),
		iterSearchUT(0)
	{}

	int lastPtrInQual;
	int firstConstPart;
	ObjectDef *inObject;
	ObjField *objField;
	ObjMethod *objMethod;
	UniqueType *uniqueType;
	UniqueType *iterSearchUT;
};

struct QualItem
{
	enum Type { Dot, Arrow };

	QualItem( const InputLoc &loc, const String &data, Type type )
		: loc(loc), data(data), type(type) {}

	InputLoc loc;
	String data;
	Type type;
};

typedef Vector<QualItem> QualItemVect;

struct LangVarRef
{
	LangVarRef( const InputLoc &loc, QualItemVect *qual, String name )
		: loc(loc), qual(qual), name(name) {}

	void resolve( Compiler *pd ) const;

	UniqueType *loadFieldInstr( Compiler *pd, CodeVect &code, ObjectDef *inObject,
			ObjField *el, bool forWriting, bool revert ) const;
	void setFieldInstr( Compiler *pd, CodeVect &code, ObjectDef *inObject, 
			ObjField *el, UniqueType *exprUT, bool revert ) const;

	VarRefLookup lookupMethod( Compiler *pd ) ;
	VarRefLookup lookupField( Compiler *pd ) const;

	VarRefLookup lookupQualification( Compiler *pd, ObjectDef *rootDef ) const;
	VarRefLookup lookupObj( Compiler *pd ) const;

	bool isCustom( Compiler *pd ) const;
	bool isLocalRef( Compiler *pd ) const;
	bool isContextRef( Compiler *pd ) const;
	void loadQualification( Compiler *pd, CodeVect &code, ObjectDef *rootObj, 
			int lastPtrInQual, bool forWriting, bool revert ) const;
	void loadCustom( Compiler *pd, CodeVect &code, 
			int lastPtrInQual, bool forWriting ) const;
	void loadLocalObj( Compiler *pd, CodeVect &code, 
			int lastPtrInQual, bool forWriting ) const;
	void loadContextObj( Compiler *pd, CodeVect &code, int lastPtrInQual, bool forWriting ) const;
	void loadGlobalObj( Compiler *pd, CodeVect &code, 
			int lastPtrInQual, bool forWriting ) const;
	void loadObj( Compiler *pd, CodeVect &code, int lastPtrInQual, bool forWriting ) const;
	void canTakeRef( Compiler *pd, VarRefLookup &lookup ) const;

	void setFieldIter( Compiler *pd, CodeVect &code, 
			ObjectDef *inObject, UniqueType *objUT, UniqueType *exprType, bool revert ) const;
	void setFieldSearch( Compiler *pd, CodeVect &code, 
			ObjectDef *inObject, UniqueType *exprType ) const;
	void setField( Compiler *pd, CodeVect &code, 
			ObjectDef *inObject, UniqueType *type, bool revert ) const;

	void assignValue( Compiler *pd, CodeVect &code, UniqueType *exprUT ) const;
	ObjField **evaluateArgs( Compiler *pd, CodeVect &code, 
			VarRefLookup &lookup, ExprVect *args ) const;
	void callOperation( Compiler *pd, CodeVect &code, VarRefLookup &lookup ) const;
	UniqueType *evaluateCall( Compiler *pd, CodeVect &code, ExprVect *args );
	UniqueType *evaluate( Compiler *pd, CodeVect &code, bool forWriting = false ) const;
	ObjField *evaluateRef( Compiler *pd, CodeVect &code, long pushCount ) const;
	ObjField *preEvaluateRef( Compiler *pd, CodeVect &code ) const;
	void resetActiveRefs( Compiler *pd, VarRefLookup &lookup, ObjField **paramRefs ) const;
	long loadQualificationRefs( Compiler *pd, CodeVect &code ) const;
	void popRefQuals( Compiler *pd, CodeVect &code, 
			VarRefLookup &lookup, ExprVect *args ) const;

	InputLoc loc;
	QualItemVect *qual;
	String name;
};

struct LangTerm
{
	enum Type {
		VarRefType,
		MethodCallType,
		NumberType,
		StringType,
		MatchType,
		NewType,
		ConstructType,
		TypeIdType,
		SearchType,
		NilType,
		TrueType,
		FalseType,
		ParseType,
		ParseStopType,
		MakeTreeType,
		MakeTokenType,
		EmbedStringType
	};

	LangTerm( Type type, LangVarRef *varRef )
		: type(type), varRef(varRef) {}

	LangTerm( LangVarRef *varRef, ExprVect *args )
		: type(MethodCallType), varRef(varRef), args(args) {}

	LangTerm( const InputLoc &loc, Type type, ExprVect *args )
		: loc(loc), type(type), args(args) {}

	LangTerm( Type type, String data )
		: type(type), varRef(0), data(data) {}

	LangTerm( Type type, NamespaceQual *nspaceQual, const String &data )
		: type(type), varRef(0), nspaceQual(nspaceQual), data(data) {}

	LangTerm( const InputLoc &loc, Type type )
		: loc(loc), type(type), varRef(0), typeRef(0) {}

	LangTerm( const InputLoc &loc, Type type, TypeRef *typeRef )
		: loc(loc), type(type), varRef(0), typeRef(typeRef) {}

	LangTerm( const InputLoc &loc, Type type, LangVarRef *varRef )
		: loc(loc), type(type), varRef(varRef) {}

	LangTerm( Type type, LangVarRef *varRef, Pattern *pattern )
		: type(type), varRef(varRef), pattern(pattern) {}

	LangTerm( const InputLoc &loc, Type type, TypeRef *typeRef, LangVarRef *varRef )
		: loc(loc), type(type), varRef(varRef), typeRef(typeRef) {}

	LangTerm( const InputLoc &loc, Type type, TypeRef *typeRef, FieldInitVect *fieldInitArgs, 
			Replacement *replacement )
		: loc(loc), type(type), typeRef(typeRef), fieldInitArgs(fieldInitArgs), 
		replacement(replacement) {}

	LangTerm( const InputLoc &loc, Type type, LangVarRef *varRef, ObjField *objField,
			TypeRef *typeRef, FieldInitVect *fieldInitArgs, Replacement *replacement )
		: loc(loc), type(type), varRef(varRef), objField(objField), typeRef(typeRef), 
			fieldInitArgs(fieldInitArgs), replacement(replacement) {}

	LangTerm( Type type, LangExpr *expr )
		: type(type), expr(expr) {}
	
	LangTerm( ReplItemList *replItemList )
		: type(EmbedStringType), replItemList(replItemList) {}

	LangTerm( const InputLoc &loc, Type type, LangVarRef *varRef,
			ObjField *objField, TypeRef *typeRef, GenericType *generic, TypeRef *parserTypeRef,
			Replacement *replacement )
		: loc(loc), type(type), varRef(varRef), objField(objField),
			typeRef(typeRef), generic(generic), parserTypeRef(parserTypeRef),
			replacement(replacement) {}
	
	void resolve( Compiler *pd );

	UniqueType *evaluateParse( Compiler *pd, CodeVect &code, bool stop ) const;
	UniqueType *evaluateNew( Compiler *pd, CodeVect &code ) const;
	UniqueType *evaluateConstruct( Compiler *pd, CodeVect &code ) const;
	UniqueType *evaluateMatch( Compiler *pd, CodeVect &code ) const;
	UniqueType *evaluate( Compiler *pd, CodeVect &code ) const;
	void assignFieldArgs( Compiler *pd, CodeVect &code, UniqueType *replUT ) const;
	UniqueType *evaluateMakeToken( Compiler *pd, CodeVect &code ) const;
	UniqueType *evaluateMakeTree( Compiler *pd, CodeVect &code ) const;
	UniqueType *evaluateEmbedString( Compiler *pd, CodeVect &code ) const;

	InputLoc loc;
	Type type;
	LangVarRef *varRef;
	ExprVect *args;
	NamespaceQual *nspaceQual;
	String data;
	ObjField *objField;
	TypeRef *typeRef;
	Pattern *pattern;
	FieldInitVect *fieldInitArgs;
	GenericType *generic;
	TypeRef *parserTypeRef;
	Replacement *replacement;
	LangExpr *expr;
	ReplItemList *replItemList;
};

struct LangExpr
{
	enum Type {
		BinaryType,
		UnaryType,
		TermType
	};

	LangExpr( const InputLoc &loc, LangExpr *left, char op, LangExpr *right )
		: loc(loc), type(BinaryType), left(left), op(op), right(right) {}

	LangExpr( const InputLoc &loc, char op, LangExpr *right )
		: loc(loc), type(UnaryType), left(0), op(op), right(right) {}

	LangExpr( LangTerm *term )
		: type(TermType), term(term) {}

	void resolve( Compiler *pd ) const;

	UniqueType *evaluate( Compiler *pd, CodeVect &code ) const;

	InputLoc loc;
	Type type;
	LangExpr *left;
	char op;
	LangExpr *right;
	LangTerm *term;
};

struct LangStmt;
typedef DList<LangStmt> StmtList;

struct LangStmt
{
	enum Type {
		AssignType,
		PrintType,
		PrintXMLACType,
		PrintXMLType,
		PrintStreamType,
		ExprType,
		IfType,
		ElseType,
		RejectType,
		WhileType,
		ReturnType,
		YieldType,
		ForIterType,
		BreakType,
		ParserType
	};

	LangStmt( const InputLoc &loc, Type type, FieldInitVect *fieldInitVect ) : 
		loc(loc), type(type), varRef(0), expr(0), fieldInitVect(fieldInitVect), next(0) {}

	LangStmt( const InputLoc &loc, Type type, ExprVect *exprPtrVect ) : 
		loc(loc), type(type), varRef(0), expr(0), exprPtrVect(exprPtrVect), next(0) {}
	
	LangStmt( const InputLoc &loc, Type type, LangExpr *expr ) : 
		loc(loc), type(type), varRef(0), expr(expr), exprPtrVect(0), next(0) {}

	LangStmt( Type type, LangVarRef *varRef ) : 
		type(type), varRef(varRef), expr(0), exprPtrVect(0), next(0) {}

	LangStmt( const InputLoc &loc, Type type, ObjField *objField ) :
		loc(loc), type(type), varRef(0), objField(objField), expr(0), 
		exprPtrVect(0), next(0) {}
	
	LangStmt( const InputLoc &loc, Type type, LangVarRef *varRef, LangExpr *expr ) : 
		loc(loc), type(type), varRef(varRef), expr(expr), exprPtrVect(0), next(0) {}
	
	LangStmt( Type type, LangExpr *expr, StmtList *stmtList ) : 
		type(type), expr(expr), stmtList(stmtList), next(0) {}

	LangStmt( Type type, StmtList *stmtList ) : 
		type(type), stmtList(stmtList), next(0) {}

	LangStmt( Type type, LangExpr *expr, StmtList *stmtList, LangStmt *elsePart ) : 
		type(type), expr(expr), stmtList(stmtList), elsePart(elsePart), next(0) {}

	LangStmt( const InputLoc &loc, Type type ) : 
		loc(loc), type(type), next(0) {}

	LangStmt( Type type, LangVarRef *varRef, Replacement *replacement ) : 
		type(type), varRef(varRef), expr(0), replacement(replacement), 
		exprPtrVect(0), next(0) {}

	LangStmt( Type type, LangVarRef *varRef, ParserText *parserText ) : 
		type(type), varRef(varRef), expr(0), parserText(parserText), 
		exprPtrVect(0), next(0) {}

	/* ForIterType */
	LangStmt( const InputLoc &loc, Type type, ObjField *objField, 
			TypeRef *typeRef, LangTerm *langTerm, StmtList *stmtList ) : 
		loc(loc), type(type), langTerm(langTerm), objField(objField), typeRef(typeRef), 
		stmtList(stmtList), next(0) {}

	LangStmt( Type type ) : 
		type(type), next(0) {}

	void resolve( Compiler *pd ) const;
	void resolveParserItems( Compiler *pd ) const;

	void evaluateParserItems( Compiler *pd, CodeVect &code ) const;
	LangTerm *chooseDefaultIter( Compiler *pd, LangTerm *fromVarRef ) const;
	void compileWhile( Compiler *pd, CodeVect &code ) const;
	void compileForIterBody( Compiler *pd, CodeVect &code, UniqueType *iterUT ) const;
	void compileForIter( Compiler *pd, CodeVect &code ) const;
	void compile( Compiler *pd, CodeVect &code ) const;

	InputLoc loc;
	Type type;
	LangVarRef *varRef;
	LangTerm *langTerm;
	ObjField *objField;
	TypeRef *typeRef;
	LangExpr *expr;
	Replacement *replacement;
	ParserText *parserText;
	ExprVect *exprPtrVect;
	FieldInitVect *fieldInitVect;
	StmtList *stmtList;
	/* Either another if, or an else. */
	LangStmt *elsePart;
	String name;

	/* Normally you don't need to initialize double list pointers, however, we
	 * make use of the next pointer for returning a pair of statements using
	 * one pointer to a LangStmt, so we need to initialize it above. */
	LangStmt *prev, *next;
};

struct CodeBlock
{
	CodeBlock( StmtList *stmtList ) 
	:
		frameId(-1),
		stmtList(stmtList),
		localFrame(0),
		context(0) {}

	void compile( Compiler *pd, CodeVect &code ) const;
	void resolve( Compiler *pd ) const;

	long frameId;
	StmtList *stmtList;
	ObjectDef *localFrame;
	CharSet trees;
	Context *context;

	/* Each frame has two versions of 
	 * the code: revert and commit. */
	CodeVect codeWV, codeWC;
};

struct Function
{
	Function( TypeRef *typeRef, const String &name, 
			ParameterList *paramList, CodeBlock *codeBlock, 
			int funcId, bool isUserIter )
	:
		typeRef(typeRef),
		name(name),
		paramList(paramList),
		codeBlock(codeBlock),
		funcId(funcId),
		isUserIter(isUserIter),
		paramListSize(0),
		paramUTs(0),
		inContext(0)
	{}

	TransBlock *transBlock;
	TypeRef *typeRef;
	String name;
	ParameterList *paramList;
	CodeBlock *codeBlock;
	ObjectDef *localFrame;
	long funcId;
	bool isUserIter;
	long paramListSize;
	UniqueType **paramUTs;
	Context *inContext;

	Function *prev, *next;
};

typedef DList<Function> FunctionList;

#endif /* _PARSETREE_H */
