%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  S::Symbol *sym;
  A::Exp *exp;
  A::ExpList *explist;
  A::Var *var;
  A::DecList *declist;
  A::Dec *dec;
  A::EFieldList *efieldlist;
  A::EField *efield;
  A::NameAndTyList *tydeclist;
  A::NameAndTy *tydec;
  A::FieldList *fieldlist;
  A::Field *field;
  A::FunDecList *fundeclist;
  A::FunDec *fundec;
  A::Ty *ty;
  }

%token<sym> ID
%token <sval> STRING
%token <ival> INT

%token 
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK 
  LBRACE RBRACE DOT 
  ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE

%left AND OR
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS

%type <exp> exp expseq opexp ifexp whileexp callexp recordexp
%type <explist> actuals  nonemptyactuals sequencing  sequencing_exps
%type <var>  lvalue one oneormore
%type <declist> decs decs_nonempty
%type <dec>  decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec>  tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one

%start program


 /*
  * Put your codes here (lab3).
  */

%%
program:  exp  {absyn_root = $1;};

opexp: exp EQ exp {$$ = new A::OpExp(errormsg.tokPos, A::EQ_OP, $1, $3);}
  | exp NEQ exp {$$ = new A::OpExp(errormsg.tokPos, A::NEQ_OP, $1, $3);}
  | exp LT exp {$$ = new A::OpExp(errormsg.tokPos, A::LT_OP, $1, $3);}
  | exp LE exp {$$ = new A::OpExp(errormsg.tokPos, A::LE_OP, $1, $3);}
  | exp GT exp {$$ = new A::OpExp(errormsg.tokPos, A::GT_OP, $1, $3);}
  | exp GE exp {$$ = new A::OpExp(errormsg.tokPos, A::GE_OP, $1, $3);}
  | exp PLUS exp {$$ = new A::OpExp(errormsg.tokPos, A::PLUS_OP, $1, $3);}
  | exp MINUS exp {$$ = new A::OpExp(errormsg.tokPos, A::MINUS_OP, $1, $3);}
  | exp TIMES exp {$$ = new A::OpExp(errormsg.tokPos, A::TIMES_OP, $1, $3);}
  | exp DIVIDE exp {$$ = new A::OpExp(errormsg.tokPos, A::DIVIDE_OP, $1, $3);}
  | MINUS exp %prec UMINUS {$$ = new A::OpExp(errormsg.tokPos, A::MINUS_OP, new A::IntExp(errormsg.tokPos, 0), $2);}
  ;

ifexp: IF exp THEN exp {$$ = new A::IfExp(errormsg.tokPos, $2, $4, nullptr);}
  | IF exp THEN exp ELSE exp {$$ = new A::IfExp(errormsg.tokPos, $2, $4, $6);}
  | IF LPAREN exp RPAREN THEN exp {$$ = new A::IfExp(errormsg.tokPos, $3, $6, nullptr);}
  | IF LPAREN exp RPAREN THEN exp ELSE exp {$$ = new A::IfExp(errormsg.tokPos, $3, $6, $8);}
  | exp AND opexp {$$ = new A::IfExp(errormsg.tokPos, $1, $3, new A::IntExp(errormsg.tokPos, 0));}
  | exp OR opexp {$$ = new A::IfExp(errormsg.tokPos, $1, new A::IntExp(errormsg.tokPos, 1), $3);}
  ;

whileexp: WHILE exp DO exp {$$ = new A::WhileExp(errormsg.tokPos, $2, $4);}
  | WHILE LPAREN exp RPAREN DO exp {$$ = new A::WhileExp(errormsg.tokPos, $3, $6);}
  ;

callexp: ID LPAREN sequencing_exps RPAREN {$$ = new A::CallExp(errormsg.tokPos, $1, $3);}
  | ID LPAREN RPAREN {$$ = new A::CallExp(errormsg.tokPos, $1, nullptr);}
  ;

recordexp: ID LBRACE RBRACE {$$ = new A::RecordExp(errormsg.tokPos, $1, nullptr);}
  | ID LBRACE rec RBRACE {$$ = new A::RecordExp(errormsg.tokPos, $1, $3);}
  ;

exp: LET decs IN expseq END {$$ = new A::LetExp(errormsg.tokPos, $2, $4);}
  | ID LBRACK exp RBRACK OF exp {$$ = new A::ArrayExp(errormsg.tokPos, $1, $3, $6);}
  | INT {$$ = new A::IntExp(errormsg.tokPos, $1);}
  | STRING {$$ = new A::StringExp(errormsg.tokPos, $1);}
  | NIL {$$ = new A::NilExp(errormsg.tokPos);}
  | BREAK {$$ = new A::BreakExp(errormsg.tokPos);}
  | lvalue {$$ = new A::VarExp(errormsg.tokPos, $1);}
  | lvalue ASSIGN exp {$$ = new A::AssignExp(errormsg.tokPos, $1, $3);}
  | LPAREN sequencing_exps RPAREN {$$ = new A::SeqExp(errormsg.tokPos, $2);}
  | LPAREN RPAREN {$$ = new A::VoidExp(errormsg.tokPos);}
  | FOR ID ASSIGN exp TO exp DO exp {$$ = new A::ForExp(errormsg.tokPos, $2, $4, $6, $8);}
  | recordexp
  | ifexp
  | whileexp
  | callexp
  | opexp
  ;

rec: rec_one {$$ = new A::EFieldList($1, nullptr);}
  | rec_one COMMA rec {$$ = new A::EFieldList($1, $3);}
  ;

rec_one: ID EQ exp {$$ = new A::EField($1, $3);}
  ;

expseq: sequencing_exps {$$ = new A::SeqExp(errormsg.tokPos, $1);}
  ;

sequencing_exps: exp {$$ = new A::ExpList($1, nullptr);}
  | exp COMMA sequencing_exps {$$ = new A::ExpList($1, $3);}
  | exp SEMICOLON sequencing_exps {$$ = new A::ExpList($1, $3);}
  ;

decs: decs_nonempty_s {$$ = new A::DecList($1, nullptr);}
  | decs_nonempty_s decs {$$ = new A::DecList($1, $2);}
  | vardec {$$ = new A::DecList($1, nullptr);}
  | vardec decs {$$ = new A::DecList($1, $2);}
  ;

decs_nonempty_s: tydec {$$ = new A::TypeDec(errormsg.tokPos, $1);}
  | fundec {$$ = new A::FunctionDec(errormsg.tokPos, $1);}
  ;

tydec: TYPE tydec_one {$$ = new A::NameAndTyList($2, nullptr);}
  | TYPE tydec_one tydec {$$ = new A::NameAndTyList($2, $3);}
  ;

fundec: FUNCTION fundec_one {$$ = new A::FunDecList($2, nullptr);}
  | FUNCTION fundec_one fundec {$$ = new A::FunDecList($2, $3);}
  ;

tydec_one: ID EQ ty {$$ = new A::NameAndTy($1, $3);}
  ;

fundec_one: ID LPAREN tyfields RPAREN COLON ID EQ exp {$$ = new A::FunDec(errormsg.tokPos, $1, $3, $6, $8);}
  | ID LPAREN RPAREN COLON ID EQ exp {$$ = new A::FunDec(errormsg.tokPos, $1, nullptr, $5, $7);}
  | ID LPAREN tyfields RPAREN EQ exp {$$ = new A::FunDec(errormsg.tokPos, $1, $3, nullptr, $6);}
  | ID LPAREN RPAREN EQ exp {$$ = new A::FunDec(errormsg.tokPos, $1, nullptr, nullptr, $5);}
  | ID LPAREN tyfields RPAREN COLON ID EQ LPAREN exp RPAREN {$$ = new A::FunDec(errormsg.tokPos, $1, $3, $6, $9);}
  | ID LPAREN RPAREN COLON ID EQ LPAREN exp RPAREN {$$ = new A::FunDec(errormsg.tokPos, $1, nullptr, $5, $8);}
  | ID LPAREN tyfields RPAREN EQ LPAREN exp RPAREN {$$ = new A::FunDec(errormsg.tokPos, $1, $3, nullptr, $7);}
  | ID LPAREN RPAREN EQ LPAREN exp RPAREN {$$ = new A::FunDec(errormsg.tokPos, $1, nullptr, nullptr, $6);}
  ;

ty: ARRAY OF ID {$$ = new A::ArrayTy(errormsg.tokPos, $3);}
  | ID {$$ = new A::NameTy(errormsg.tokPos, $1);}
  | LBRACE tyfields RBRACE {$$ = new A::RecordTy(errormsg.tokPos, $2);}
  ;

tyfields: ID COLON ID {$$ = new A::FieldList(new A::Field(errormsg.tokPos, $1, $3), nullptr);}
  | ID COLON ID COMMA tyfields {$$ = new A::FieldList(new A::Field(errormsg.tokPos, $1, $3), $5);}
  ;

lvalue: lvalue DOT ID {$$ = new A::FieldVar(errormsg.tokPos, $1, $3);}
  | ID LBRACK exp RBRACK {$$ = new A::SubscriptVar(errormsg.tokPos, new A::SimpleVar(errormsg.tokPos, $1), $3);}
  | lvalue LBRACK exp RBRACK {$$ = new A::SubscriptVar(errormsg.tokPos, $1, $3);}
  | ID {$$ = new A::SimpleVar(errormsg.tokPos, $1);}
  ;

vardec: VAR ID ASSIGN exp {$$ = new A::VarDec(errormsg.tokPos,$2, nullptr, $4);}
  | VAR ID COLON ID ASSIGN exp {$$ = new A::VarDec(errormsg.tokPos, $2, $4, $6);}
  ;