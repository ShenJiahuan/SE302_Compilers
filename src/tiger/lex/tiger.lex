%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */

%x COMMENT STR

%%

 /*
  * TODO: Put your codes here (lab2).
  *
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg.Newline();}

 /* reserved words */

\" {
  begin(StartCondition__::STR);
  adjust();
  stringBuf_ = "";
}

<STR> {
  \" {
    begin(StartCondition__::INITIAL);
    std::string str = matched();
    adjustStr();
    setMatched(stringBuf_);
    return Parser::STRING;
  }

  \\n {
    std::string str = matched();
    adjustStr();
    stringBuf_ += "\n";
  }

  \\t {
    std::string str = matched();
    adjustStr();
    stringBuf_ += "\t";
  }

  \\\" {
    std::string str = matched();
    adjustStr();
    stringBuf_ += "\"";
  }

  \\\\ {
    std::string str = matched();
    adjustStr();
    stringBuf_ += "\\";
  }

  \\[ \t\n]+\\ {
    std::string str = matched();
    adjustStr();
  }

  \\[0-9]{3} {
    std::string str = matched();
    adjustStr();
    str = atoi(str.substr(1).c_str());
    stringBuf_ += str;
  }

  \\\^C {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(3));
  }

  \\\^O {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(15));
  }

  \\\^M {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(13));
  }

  \\\^P {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(16));
  }

  \\\^I {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(9));
  }

  \\\^L {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(12));
  }


  \\\^E {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(5));
  }

  \\\^R {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(18));
  }

  \\\^S {
    std::string str = matched();
    adjustStr();
    stringBuf_ += std::string(1, char(19));
  }

  \\.|. {
    std::string str = matched();
    adjustStr();
    stringBuf_ += str;
  }
}

\/\* {
  commentLevel_++;
  begin(StartCondition__::COMMENT);
  adjust();
}

<COMMENT> {
  \*\/ {
    commentLevel_--;
    if (commentLevel_ == 0) {
      begin(StartCondition__::INITIAL);
    }
    adjust();
  }

  \/\* {
    commentLevel_++;
    begin(StartCondition__::COMMENT);
    adjust();
  }

  \\.|.|\n {adjust();}
}

[0-9]+ {adjust(); return Parser::INT;}

, {adjust(); return Parser::COMMA;}

: {adjust(); return Parser::COLON;}

; {adjust(); return Parser::SEMICOLON;}

\( {adjust(); return Parser::LPAREN;}

\) {adjust(); return Parser::RPAREN;}

\[ {adjust(); return Parser::LBRACK;}

\] {adjust(); return Parser::RBRACK;}

\{ {adjust(); return Parser::LBRACE;}

\} {adjust(); return Parser::RBRACE;}

\. {adjust(); return Parser::DOT;}

\+ {adjust(); return Parser::PLUS;}

\- {adjust(); return Parser::MINUS;}

\* {adjust(); return Parser::TIMES;}

\/ {adjust(); return Parser::DIVIDE;}

= {adjust(); return Parser::EQ;}

"<>" {adjust(); return Parser::NEQ;}

"<" {adjust(); return Parser::LT;}

"<=" {adjust(); return Parser::LE;}

">" {adjust(); return Parser::GT;}

">=" {adjust(); return Parser::GE;}

"&" {adjust(); return Parser::AND;}

"|" {adjust(); return Parser::OR;}

":=" {adjust(); return Parser::ASSIGN;}

"array" {adjust(); return Parser::ARRAY;}

"if" {adjust(); return Parser::IF;}

"then" {adjust(); return Parser::THEN;}

"else" {adjust(); return Parser::ELSE;}

"while" {adjust(); return Parser::WHILE;}

"for" {adjust(); return Parser::FOR;}

"to" {adjust(); return Parser::TO;}

"do" {adjust(); return Parser::DO;}

"let" {adjust(); return Parser::LET;}

"in" {adjust(); return Parser::IN;}

"end" {adjust(); return Parser::END;}

"of" {adjust(); return Parser::OF;}

"break" {adjust(); return Parser::BREAK;}

"nil" {adjust(); return Parser::NIL;}

"function" {adjust(); return Parser::FUNCTION;}

"var" {adjust(); return Parser::VAR;}

"type" {adjust(); return Parser::TYPE;}

[A-Za-z][A-Za-z0-9_]* {adjust(); return Parser::ID;}

. {adjust(); errormsg.Error(errormsg.tokPos, "illegal token");}
