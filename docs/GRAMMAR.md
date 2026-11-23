# مستندات رسمی فاز اول — زبان B-Minor

> توجه: فایل صورت پروژه خودِ مشخصات زبان B-Minor را نداشت؛ مجموعه کلیدواژه‌ها، عملگرها و اولویت‌ها بر پایهٔ
> B-Minor استاندارد (و نمونه‌های داخل PDF) نوشته شده است. اگر مشخصات درس تفاوت دارد، فقط جدول‌های بخش ۱ و ۵ را عوض کنید.

## ۱. تعریف رسمی توکن‌ها (عبارت منظم)

| دسته | عبارت منظم | نکته |
|---|---|---|
| Whitespace | `[ \t\r\n\f\v]+` | دور ریخته می‌شود؛ فقط برای محاسبهٔ خط/ستون |
| Line comment | `//[^\n]*` | تا آخر خط |
| Block comment | `/\*([^*]\|\*+[^*/])*\*+/` | بدون تودرتو؛ بسته‌نشده ⇒ خطای لغوی |
| Keyword | `array\|boolean\|char\|else\|for\|function\|if\|integer\|print\|return\|string\|void\|while` | |
| Boolean literal | `true\|false` | |
| Identifier | `[a-zA-Z_][a-zA-Z0-9_]*` | بعد از رد شدن از جدول کلیدواژه‌ها |
| Integer literal | `[0-9]+` | علامت منفی عملگر یکانی است (پارسر)، تا `n-1` درست شکسته شود |
| String literal | `"([^"\\\n]\|\\[ntr0\\"'])*"` | escape نامعتبر ⇒ خطا، ولی توکن STRING می‌ماند |
| Char literal | `'([^'\\\n]\|\\[ntr0\\"'])'` | |
| Operator | `+ - * / % ^ < <= > >= == != && \|\| ! ++ --` | |
| Assign | `=` | |
| Delimiter | `( ) { } [ ] , ; :` | هر کدام توکن جدا |
| INVALID | هر کاراکتر دیگر (مثل `@`، `&` و `\|` تنها)، رشتهٔ بسته‌نشده، کاراکتر بسته‌نشده | |

### DFA
لکسر یک DFA دست‌نویس است: حالت شروع روی اولین کاراکتر شاخه می‌شود (حرف/`_` ← حالت شناسه، رقم ← حالت عدد،
`"` ← رشته، `'` ← کاراکتر، `/` ← حالت تصمیم‌گیری بین `/`، `//`، `/*`، بقیه ← جدول عملگرها). هر حالت پذیرنده
تا جایی که انتقال دارد ادامه می‌دهد و در آخرین حالت پذیرش توکن را می‌سازد؛ این همان نتیجهٔ ساخت تامپسون + زیرمجموعه‌ها است.

### انتخاب توکن نهایی
1. **Longest match (Maximal Munch):** عملگرهای دوکاراکتری قبل از تک‌کاراکتری‌ها بررسی می‌شوند، پس `<=` یک توکن است نه `<` و `=`؛
   همچنین `returned` یک IDENT است نه `return` + `ed`، و `12ab` ← `INT(12) IDENT(ab)`.
2. **Priority:** پس از خواندن یک لکسم به شکل شناسه، ابتدا جدول کلیدواژه‌ها / `true` / `false` بررسی می‌شود؛ پس `return` ⇒ `KW(return)`.
3. `//` و `/*` بر عملگر `/` اولویت دارند (طولانی‌ترین تطبیق).

### خطاهای لغوی
هرگز توقف نمی‌کند: کاراکتر ناشناخته ⇒ `INVALID` با موقعیت دقیق، عبور از همان کاراکتر (یک کاراکتر UTF-8 کامل) و ادامه.
رشتهٔ بسته‌نشده تا پایان خط و کامنت بلوکی بسته‌نشده تا پایان فایل به‌صورت یک خطا گزارش می‌شوند.

## ۲. گرامر EBNF

```ebnf
program     ::= decl* EOF
decl        ::= IDENT ':' ( func_decl | var_decl )
func_decl   ::= 'function' type '(' param_list? ')' ( ';' | '=' block )
var_decl    ::= type ( '=' ( init_list | expr ) )? ';'
param_list  ::= param ( ',' param )*
param       ::= IDENT ':' type
type        ::= 'integer' | 'boolean' | 'char' | 'string' | 'void'
              | 'array' '[' expr? ']' type
init_list   ::= '{' ( init_item ( ',' init_item )* )? '}'
init_item   ::= init_list | expr

block       ::= '{' stmt* '}'
stmt        ::= block | decl                       (* decl iff  IDENT ':'  — LL(2) *)
              | 'if' '(' expr ')' stmt ( 'else' stmt )?
              | 'for' '(' expr? ';' expr? ';' expr? ')' stmt
              | 'while' '(' expr ')' stmt
              | 'return' expr? ';'
              | 'print' ( expr ( ',' expr )* )? ';'
              | expr ';'

expr        ::= or ( '=' expr )?                   (* راست‌شرکت‌پذیر؛ هدف باید IDENT یا index باشد *)
or          ::= and ( '||' and )*
and         ::= cmp ( '&&' cmp )*
cmp         ::= add ( ( '<' | '<=' | '>' | '>=' | '==' | '!=' ) add )*
add         ::= mul ( ( '+' | '-' ) mul )*
mul         ::= pow ( ( '*' | '/' | '%' ) pow )*
pow         ::= unary ( '^' pow )?                 (* راست‌شرکت‌پذیر *)
unary       ::= ( '-' | '!' ) unary | postfix
postfix     ::= primary ( '++' | '--' | '[' expr ']' | '(' ( expr ( ',' expr )* )? ')' )*
primary     ::= INT | STRING | CHAR | BOOL | IDENT | '(' expr ')'
```

اولویت (از کم به زیاد): `=` ، `||` ، `&&` ، مقایسه‌ها ، `+ -` ، `* / %` ، `^` ، یکانی `- !` ، پسوندی `++ -- [] ()`.

## ۳. روش پارس: بازگشتی نزولی LL(1)
* برای هر غیرپایانه یک تابع (`Parser::parseX` در `src/parser.cpp`).
* **بازگشت چپ:** شکل طبیعی `add ::= add '+' mul | mul` بازگشت چپ دارد و حذف شد:
  `add ::= mul ( ('+'|'-') mul )*` (حلقه به‌جای بازگشت). همین برای `or/and/cmp/mul/postfix`؛ در کد با تابع `leftAssoc`.
  `^` و `=` عمداً بازگشت راست دارند.
* **FIRST/FOLLOW و تعارض‌ها:**
  * FIRST(type) = {integer, boolean, char, string, void, array}؛ FIRST(expr) = {`-`, `!`, INT, STRING, CHAR, BOOL, IDENT, `(`}.
  * `decl`: بعد از `:` کلیدواژهٔ `function` با FIRST(type) جدا است؛ بعد از type، `'='` و `';'` جدا هستند؛ بعد از `=` علامت `{` شروع init_list است و با FIRST(expr) تداخل ندارد.
  * تنها نقطهٔ LL(2): در `stmt` دو توکن `IDENT ':'` یعنی تعریف، و `IDENT` به‌تنهایی شروع عبارت است.
  * **dangling else:** تعارض FIRST/FOLLOW روی `else` با قاعدهٔ «else به نزدیک‌ترین if» (انتخاب shift) حل شده است.
  * `print` و `return` با `;` بعدشان بدون ابهام‌اند چون `;` در FIRST(expr) نیست.

## ۴. بازیابی خطا (Panic-Mode)
* خطای «توکن غیرمنتظره در ابتدای عبارت/نوع/تعریف» ⇒ استثنای داخلی ⇒ نزدیک‌ترین نقطهٔ بازیابی (`parseStmtSafe` یا حلقهٔ سطح بالا) یک گره `Error` می‌گذارد و `synchronize` توکن‌ها را رد می‌کند تا:
  `;` (مصرف می‌شود)، `}` (باقی می‌ماند)، کلیدواژهٔ شروع دستور (`if for while return print`) یا `IDENT :`.
* خطاهای «توکن بسته/جداکننده کم است» (`)` `]` `;` `}`) با **درج مجازی توکن** گزارش و ادامه داده می‌شوند (بدون پرش)، مثل `if (y > 0 {`.
* برای هر توکن حداکثر یک خطا گزارش می‌شود (جلوگیری از خطاهای زنجیره‌ای) و هر دور بازیابی حداقل یک توکن پیش می‌رود (حلقهٔ بی‌نهایت ممکن نیست).
* توکن‌های `INVALID` که لکسر گزارش کرده به پارسر داده نمی‌شوند.

## ۵. AST
گره‌ها (`src/ast.hpp`): `Program, VarDecl, FuncDecl, Param, PrimType, ArrayType, FuncType, Block, ExprStmt, PrintStmt, ReturnStmt, IfStmt, ForStmt, WhileStmt, AssignExpr, BinaryExpr, UnaryExpr, PostfixExpr, IndexExpr, CallExpr, Identifier, IntLiteral, StringLiteral, CharLiteral, BoolLiteral, InitList, Error`.
هر گره: `loc` (خط و ستون اولین توکن)، `typeAnn` (خالی = null؛ در فاز ۲ پر می‌شود)، و `tokIdx` (اندیس توکن اصلی برای رنگ‌آمیزی).
چاپ درخت: `./bminor --ast file.bm`.

## ۶. رنگ‌آمیز نحوی
۱) رنگ پایه از نوع توکن؛ ۲) پیمایش AST: نام در `FuncDecl` و callee یک `CallExpr` ⇒ **Function**، `Identifier/VarDecl/Param` ⇒ **Variable**،
`PrimType` ⇒ **Type**؛ ۳) توکن‌های دارای خطا (لغوی یا نحوی) ⇒ **قرمز + زیرخط** با پیام در `title` (HTML).
خروجی از متن اصلی بازسازی می‌شود (فاصله‌ها و کامنت‌ها دقیقاً حفظ می‌شوند).

| دسته | ANSI | کلاس CSS |
|---|---|---|
| کلیدواژه | آبی پررنگ `1;34` | `kw` |
| نام نوع | فیروزه‌ای `36` | `ty` |
| متغیر | پیش‌فرض | `var` |
| تابع | زرد `33` | `fn` |
| عدد / بولی | نارنجی | `lit num` / `lit bool` |
| رشته / کاراکتر | سبز | `lit str` |
| عملگر و جداکننده | خاکستری روشن | `op` |
| توضیح | خاکستری کم‌رنگ ایتالیک | `cmt` |
| خطا | قرمز + زیرخط | `err` |
